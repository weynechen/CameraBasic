#include "camera.hpp"

#pragma comment(lib, "strmiids")

//define release maco
#define ReleaseInterface(x) \
	if ( NULL != x ) \
{ \
	x->Release( ); \
	x = NULL; \
}
// Application-defined message to notify app of filter graph events
#define WM_GRAPHNOTIFY  WM_APP+100

Camera::Camera():
	mInitOK(false),
	mVideoHeight(0),
	mVideoWidth(0),
	mDevFilter(NULL),
	mCaptureGB(NULL),
	mGraphBuilder(NULL),
	mMediaControl(NULL),
	mMediaEvent(NULL),
	mSampGrabber(NULL),
	mIsVideoOpened(false)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (SUCCEEDED(hr))
	{
		mInitOK = true;
	}
}

Camera::~Camera()
{
	Close();
	CoUninitialize();
}

HRESULT Camera::InitializeEnv()
{
	HRESULT hr;

	//Create the filter graph
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
		IID_IGraphBuilder, (LPVOID*)&mGraphBuilder);
	if (FAILED(hr))
		return hr;

	//Create the capture graph builder
	hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER,
		IID_ICaptureGraphBuilder2, (LPVOID*)&mCaptureGB);
	if (FAILED(hr))
		return hr;

	//Obtain interfaces for media control and Video Window
	hr = mGraphBuilder->QueryInterface(IID_IMediaControl, (LPVOID*)&mMediaControl);
	if (FAILED(hr))
		return hr;


	hr = mGraphBuilder->QueryInterface(IID_IMediaEventEx, (LPVOID*)&mMediaEvent);
	if (FAILED(hr))
		return hr;

	mCaptureGB->SetFiltergraph(mGraphBuilder);
	if (FAILED(hr))
		return hr;
	return hr;
}

std::vector<std::wstring> Camera::EnumAllCamera(void)
{
	if (mInitOK == false)
		return std::vector<std::wstring>();

	std::vector<std::wstring> names;
	IEnumMoniker *pEnum;
	// Create the System Device Enumerator.
	ICreateDevEnum *pDevEnum;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
		CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

	if (SUCCEEDED(hr))
	{
		// Create an enumerator for the category.
		hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
		if (hr == S_FALSE)
		{
			hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
		}
		pDevEnum->Release();
	}

	if (!SUCCEEDED(hr))
		return std::vector<std::wstring>();

	IMoniker *pMoniker = NULL;
	while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
	{
		IPropertyBag *pPropBag;
		IBindCtx* bindCtx = NULL;
		LPOLESTR str = NULL;
		VARIANT var;
		VariantInit(&var);

		HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
		if (FAILED(hr))
		{
			pMoniker->Release();
			continue;
		}

		// Get description or friendly name.
		hr = pPropBag->Read(L"Description", &var, 0);
		if (FAILED(hr))
		{
			hr = pPropBag->Read(L"FriendlyName", &var, 0);
		}
		if (SUCCEEDED(hr))
		{
			names.push_back(var.bstrVal);
			VariantClear(&var);
		}

		pPropBag->Release();
		pMoniker->Release();
	}

	pEnum->Release();

	return names;
}


HRESULT Camera::BindFilter(int deviceID, IBaseFilter **pBaseFilter)
{
	ICreateDevEnum *pDevEnum;
	IEnumMoniker   *pEnumMon;
	IMoniker	   *pMoniker;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
		IID_ICreateDevEnum, (LPVOID*)&pDevEnum);
	if (SUCCEEDED(hr))
	{
		hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumMon, 0);
		if (hr == S_FALSE)
		{
			hr = VFW_E_NOT_FOUND;
			return hr;
		}
		pEnumMon->Reset();
		ULONG cFetched;
		int index = 0;
		hr = pEnumMon->Next(1, &pMoniker, &cFetched);
		while (hr == S_OK && index <= deviceID)
		{
			IPropertyBag *pProBag;
			hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (LPVOID*)&pProBag);
			if (SUCCEEDED(hr))
			{
				if (index == deviceID)
				{
					pMoniker->BindToObject(0, 0, IID_IBaseFilter, (LPVOID*)pBaseFilter);
				}
			}
			pMoniker->Release();
			index++;
			hr = pEnumMon->Next(1, &pMoniker, &cFetched);
		}
		pEnumMon->Release();
	}
	return hr;
}


bool Camera::Open(std::wstring &camera_name)
{
	if (mInitOK == false)
		return false;

	if (mIsVideoOpened)
		return true;
	
	HRESULT hr;

#define CHECK_HR(x) do{ hr = (x); if (FAILED(hr)){ Close(); return false;}}while(0)

	CHECK_HR(InitializeEnv());
	
	IBaseFilter *pSampleGrabberFilter , *dest_filter;

	std::vector<std::wstring> names = EnumAllCamera();

	if (names.empty())
	{
		Close(); return false;
	}

	auto name_it = find(names.begin(), names.end(), camera_name);
	if (name_it == names.end())
	{
		Close(); return false;
	}

	int deviceID = static_cast<int>(distance(names.begin(), name_it));


	// create grabber filter instance
	CHECK_HR(CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
		IID_IBaseFilter, (LPVOID*)&pSampleGrabberFilter));

	// bind source device
	CHECK_HR(BindFilter(deviceID, &mDevFilter));
	
	// add src filter
	CHECK_HR(mGraphBuilder->AddFilter(mDevFilter, L"Video Filter"));
	
	// add grabber filter and query interface
	CHECK_HR(mGraphBuilder->AddFilter(pSampleGrabberFilter, L"Sample Grabber"));
	
	CHECK_HR(pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (LPVOID*)&mSampGrabber));
	
	// find the current bit depth
	HDC hdc = GetDC(NULL);
	mBitDepth = GetDeviceCaps(hdc, BITSPIXEL);
	ReleaseDC(NULL, hdc);

	// set the media type for grabber filter
	AM_MEDIA_TYPE mediaType;
	ZeroMemory(&mediaType, sizeof(AM_MEDIA_TYPE));
	mediaType.majortype = MEDIATYPE_Video;
	switch (mBitDepth)
	{
	case  8:
		mediaType.subtype = MEDIASUBTYPE_RGB8;
		break;
	case 16:
		mediaType.subtype = MEDIASUBTYPE_RGB555;
		break;
	case 24:
		mediaType.subtype = MEDIASUBTYPE_RGB24;
		break;
	case 32:
		mediaType.subtype = MEDIASUBTYPE_RGB32;
		break;
	default:
		Close();
		return false;
	}
	mediaType.formattype = FORMAT_VideoInfo;
	hr = mSampGrabber->SetMediaType(&mediaType);

	CHECK_HR(CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&dest_filter)));
	mGraphBuilder->AddFilter(dest_filter, L"NullRenderer");

	// connect source filter to grabber filter
	CHECK_HR(mCaptureGB->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
		mDevFilter, pSampleGrabberFilter, dest_filter));
	
	// get connected media type
	CHECK_HR(mSampGrabber->GetConnectedMediaType(&mediaType));
	VIDEOINFOHEADER * vih = (VIDEOINFOHEADER*)mediaType.pbFormat;
	mVideoWidth = vih->bmiHeader.biWidth;
	mVideoHeight = vih->bmiHeader.biHeight;
	
	// configure grabber filter
	CHECK_HR(mSampGrabber->SetOneShot(0));
	
	CHECK_HR(mSampGrabber->SetBufferSamples(0));
	
	// Use the BufferCB callback method
	CHECK_HR(mSampGrabber->SetCallback(&mSampleGrabberCB, 1));

	mSampleGrabberCB.mNewDataCallBack = mFrameCallBack;

	mMediaControl->Run();
	dest_filter->Release();
	pSampleGrabberFilter->Release();
	
	// release resource
	if (mediaType.cbFormat != 0)
	{
		CoTaskMemFree((PVOID)mediaType.pbFormat);
		mediaType.cbFormat = 0;
		mediaType.pbFormat = NULL;
	}
	if (mediaType.pUnk != NULL)
	{
		mediaType.pUnk->Release();
		mediaType.pUnk = NULL;
	}
	mIsVideoOpened = TRUE;
	return true;
}

bool Camera::Close()
{
	if (mMediaControl)
	{
		mMediaControl->Stop();
	}
	if (mMediaEvent)
	{
		mMediaEvent->SetNotifyWindow(NULL, WM_GRAPHNOTIFY, 0);
	}
	mIsVideoOpened = false;
	//release interface
	ReleaseInterface(mDevFilter);
	ReleaseInterface(mCaptureGB);
	ReleaseInterface(mGraphBuilder);
	ReleaseInterface(mMediaControl);
	ReleaseInterface(mMediaEvent);
	ReleaseInterface(mSampGrabber);

	return true;
}

void Camera::SetCallBack(std::function<void(double, BYTE*, LONG)> f)
{
	mFrameCallBack = f;
}

ULONG STDMETHODCALLTYPE Camera::SampleGrabberCallback::AddRef()
{
	return 1;
}

ULONG STDMETHODCALLTYPE Camera::SampleGrabberCallback::Release()
{
	return 2;
}

HRESULT STDMETHODCALLTYPE Camera::SampleGrabberCallback::QueryInterface(REFIID riid, void** ppvObject)
{
	if (NULL == ppvObject) return E_POINTER;
	if (riid == __uuidof(IUnknown))
	{
		*ppvObject = static_cast<IUnknown*>(this);
		return S_OK;
	}
	if (riid == IID_ISampleGrabberCB)
	{
		*ppvObject = static_cast<ISampleGrabberCB*>(this);
		return S_OK;
	}
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE Camera::SampleGrabberCallback::SampleCB(double Time, IMediaSample *pSample)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE Camera::SampleGrabberCallback::BufferCB(double Time, BYTE * pBuffer, long BufferLen)
{
	if (mNewDataCallBack)
	{
		mNewDataCallBack(Time, pBuffer, BufferLen);
	}
	return S_OK;
}


