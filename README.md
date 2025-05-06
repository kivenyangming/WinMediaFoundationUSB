# USB Camera NV12 Capture via Windows Media Foundation

该项目演示了如何使用 **Windows Media Foundation (MF) API** 从USB摄像头捕获视频流，并将图像数据保存为 **NV12格式** 的二进制文件。

## 功能特性
- 🎥 枚举系统中可用的USB摄像头设备  
- ⚙️ 配置摄像头分辨率及媒体类型（支持原生格式）  
- 📸 捕获连续视频帧并保存为二进制文件（NV12格式）  
- 🧹 自动释放COM对象及资源  

## 依赖项
- **Windows SDK**（需包含Media Foundation头文件）  
- **Visual Studio 2019+**（支持C++17）  
- 系统需启用 **Windows Media Foundation** 组件  

## 构建说明
1. 使用Visual Studio创建新的 **C++控制台项目**  
2. 将 `main.cpp` 添加到项目中  
3. 配置项目属性：  
   - **附加库目录**：添加Windows SDK的`Lib`路径（如 `$(WindowsSdkDir)Lib\$(WindowsSDKVersion)\um\<arch>`)  
   - **附加依赖项**：  
     ```
     mfplat.lib;mf.lib;mfuuid.lib;mfreadwrite.lib;shlwapi.lib;strmiids.lib;
     ```

## 使用方法
1. **编译运行**：程序将自动检测第一个可用摄像头  
2. **输出文件**：捕获的60帧数据将保存至 `NV12BIN.bin`（默认路径需在代码中修改）  
3. **参数调整**：  
   - `index_devices`：选择摄像头索引（默认为0）  
   - `index_pix`：选择媒体类型索引（通过日志查看支持的分辨率）  
   - `bufferSize`：根据目标分辨率调整（计算方式：`宽×高×1.5`）  

## 文件结构
- `main.cpp`：主程序入口，包含以下核心函数：  
  - `init_usb()`：初始化摄像头及MF环境  
  - `get_pix()`：获取单帧NV12数据,可修改为其它格式数据
  - `release_usb()`：释放资源  

## 注意事项
- 🔒 需以 **管理员权限** 运行（部分摄像头驱动需要特权）  
- 📍 修改代码中硬编码的文件保存路径（`C:/Users/kiven/Desktop/...`）  
- ⚠️ 确保摄像头未被其他程序占用  
- 💡 通过日志输出的分辨率列表选择合适`index_pix`  

## 兼容性
- 支持 **Windows 10/11**  
- 已测试环境：Visual Studio 2022 + Windows SDK 10.0.19041.0  

## 许可证
待补充（默认遵循MIT License，请根据需求添加）  

---

**提示**：使用第三方工具（如FFmpeg）可将NV12二进制文件转换为可视格式（如YUV或MP4）。  
示例命令：  
`ffmpeg -f rawvideo -pix_fmt nv12 -s 1920x1080 -i NV12BIN.bin output.mp4`
