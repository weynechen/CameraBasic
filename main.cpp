#include "camera.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <string>

using namespace std;
using namespace chrono;



static int m_lWidth, m_lHeight, m_iBitCount;

BOOL SaveBitmap(BYTE *pBuffer, long lBufferSize)
{
	HANDLE hf = CreateFile("test.bmp", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
	{
		return E_FAIL;
	}

	BITMAPFILEHEADER bfh;  //Set bitmap header
	ZeroMemory(&bfh, sizeof(bfh));
	bfh.bfType = 'MB';
	bfh.bfSize = sizeof(bfh) + lBufferSize + sizeof(BITMAPFILEHEADER);
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPFILEHEADER);
	// Write the file header.
	DWORD dwWritten = 0;
	WriteFile(hf, &bfh, sizeof(bfh), &dwWritten, NULL);
	// Write the file Format
	BITMAPINFOHEADER bih;
	ZeroMemory(&bih, sizeof(bih));
	bih.biSize = sizeof(bih);
	bih.biWidth = m_lWidth;
	bih.biHeight = m_lHeight;
	bih.biPlanes = 1;
	bih.biBitCount = m_iBitCount;  //Specifies the number of bits per pixel (bpp)
	WriteFile(hf, &bih, sizeof(bih), &dwWritten, NULL);
	//Write the file Data
	WriteFile(hf, pBuffer, lBufferSize, &dwWritten, NULL);
	CloseHandle(hf);

	return 0;
}


void GetFrame(Camera *camera, BYTE *buff, LONG len)
{
	static auto last_time = system_clock::now();
	static auto last_avg_time = system_clock::now();
	static int32_t dt_avg_v = 0, total_count = 0;
	auto cur_time = system_clock::now();
	auto dt = duration_cast<milliseconds>(cur_time - last_time);

	total_count++;

	if (total_count % 30 == 0) {
		auto cur_avg_time = system_clock::now();
		auto dt_avg = duration_cast<milliseconds>(cur_avg_time - last_avg_time);
		dt_avg_v = (int32_t)dt_avg.count();
		last_avg_time = cur_avg_time;
	}

	last_time = cur_time;
	int32_t dt_v = (int32_t)dt.count();
	printf("frame size: h = %d, w = %d, fps_rt: %0.02f fps: %0.02f (%d)\n",
		camera->GetHeight(), camera->GetWidth(), 1000.0f / dt_v,
		dt_avg_v ? 1000.0f * 30 / dt_avg_v : 0, total_count);

	if (total_count == 100)
	{
		m_lWidth = camera->GetWidth();
		m_lHeight = camera->GetHeight();
		m_iBitCount = camera->GetBitDepth();
		SaveBitmap(buff, len);
	}
}

int main(int argc, char **argv)
{
	Camera *camera = Camera::GetInstance();
	vector<wstring>  names = camera->EnumAllCamera();
	wstring name;
	if (names.empty() == false)
	{
		for (auto s : names)
		{
			wcout << s << endl;
			name = s;
		}
	}

	camera->SetCallBack([&camera](double, BYTE* buff, LONG len) {GetFrame(camera, buff, len); });

	int n = 0;
	//test open and close
	while (n < 4)
	{
		n++;
		camera->Open(name);
		this_thread::sleep_for(chrono::seconds(5));
		camera->Close();
	}

	return 0;
}
