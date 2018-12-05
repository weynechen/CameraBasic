​	使用DirectShow获取摄像头数据，数据在回调函数中进行处理。回调函数原型如下：

```c

	/*!
	* @param time : Starting time of the sample, in seconds.
	* @param buff : Pointer to a buffer that contains the sample data.
	* @param len  : Length of the buffer pointed to by pBuffer, in bytes.
	*/
	void SetCallBack(std::function<void(double time, BYTE *buff, LONG len)>);

```

### 使用方法

1. 枚举系统安装的摄像头，获取其名称。这里的名称一般用来在UI上显示，用户通过UI选择对应的名称。
2. 设置回调函数。
3. 传入摄像头名称，打开摄像头。打开后会开启一个线程，当有新的帧来临时，会调用回调函数。
4. 在回调函数中处理捕获的数据。
5. 关闭摄像头。

### 注意

这里是一个单例类，所以一次只能打开一个摄像头。我只在win10上使用vs2017测试过，其他的理论上也是可以的。

## EN

Using DirectShow to get camera data.Data will be handled in the callback function.

### HOW  TO  USE

1. Enumerate all camera in the computer , get friendly name of camera . The name will be show on UI , and user can select camera name to open it.
2. Set callback function.
3. Call `open` function ,open a new thread ,an when a new frame come , this thread will call callback function.
4. You should handle data in callback function.
5. Close camera.

### Note

This is singleton class , which means you only can open one camera each times.I only test on windows 10.

