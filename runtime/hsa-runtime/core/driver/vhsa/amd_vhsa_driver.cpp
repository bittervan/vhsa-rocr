#include "core/inc/amd_vhsa_driver.hpp"

// --- 核心依赖 ---
#include "core/inc/driver.h"
#include "core/inc/runtime.h"
#include "core/inc/amd_gpu_agent.h" // 我们需要用它来创建虚拟 Agent
#include "core/util/os.h"       // 使用 ROCR 的打印工具
#include "core/inc/memory_region.h"
#include "hsa.h"

// --- 用于用户态驱动的系统头文件 ---
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <dirent.h>
#include <cstring>
#include <string>

// 使用我们之前约定的命名空间
namespace rocr {
namespace AMD {

// === 这是我们之前讨论的、需要在 ROCR 运行时中注册的“发现函数” ===
// 这是我们整个 vHSA 模块的入口点
hsa_status_t AMD::VhsaDriver::DiscoverDriver(std::unique_ptr<core::Driver>& driver) {
    auto tmp_driver = std::unique_ptr<core::Driver>(new VhsaDriver("/dev/vhsa"));

    if (tmp_driver->Open() == HSA_STATUS_SUCCESS) {
        driver = std::move(tmp_driver);
        return HSA_STATUS_SUCCESS;
    }

    return HSA_STATUS_ERROR;
}


// --- VhsaDriver 类的成员函数实现 ---

VhsaDriver::VhsaDriver(std::string devnode_name)
    // 调用父类构造函数，将我们的驱动类型标识为 KFD，因为我们要模拟的就是一个 KFD 设备
    : core::Driver(core::DriverType::KFD, devnode_name) 
{
    printf("vHSA: VhsaDriver constructor called for device path: %s\n", devnode_name.c_str());
}

VhsaDriver::~VhsaDriver() {
    // 析构函数：确保资源被正确释放
    if (is_open_) {
        Close();
    }
}

hsa_status_t VhsaDriver::Open() {
    // TODO (vHSA): 在这里实现 mmap /dev/mem 来访问 QEMU 设备的 BAR 空间
    // 这是用户态驱动的核心步骤。
    // 1. 读取 resource0 文件获取物理地址和大小。
    // 2. 打开 /dev/mem。
    // 3. 调用 mmap 将物理地址映射到本进程的虚拟地址空间。
    // 4. 将映射后的指针存到私有成员变量 qemu_device_mmap_ 中。
    
    printf("vHSA: Open() called. Simulating mmap to QEMU device...\n");
    is_open_ = true; // 假设成功
    return HSA_STATUS_SUCCESS;
}

hsa_status_t VhsaDriver::Close() {
    // TODO (vHSA): 实现 munmap 和 close(/dev/mem)
    printf("vHSA: Close() called. Simulating munmap...\n");
    // if (qemu_device_mmap_) munmap(qemu_device_mmap_, ...);
    // if (qemu_device_fd_ >= 0) close(qemu_device_fd_);
    is_open_ = false;
    return HSA_STATUS_SUCCESS;
}


hsa_status_t VhsaDriver::Init() {
    // 这是核心初始化逻辑。ROCR 会调用这个函数。
    // 我们的目标是：在这里发现所有由宿主机转发过来的“虚拟Agent”。

    printf("vHSA: Driver Init() called.\n");
    
    // TODO (vHSA): 实现与 QEMU 后端的通信
    // 1. 通过 MMIO 向 QEMU vHSA 设备发送一个 "VHSA_CMD_GET_AGENTS" 命令。
    // 2. QEMU 后端在宿主机上调用真实的 hsa_iterate_agents，获取物理 GPU 的属性。
    // 3. QEMU 后端将属性数据打包，通过共享内存或 MMIO 返回。
    // 4. 在这里接收属性数据。
    // 5. 根据接收到的数据，`new` 一个或多个 `AMD::GpuAgent` 对象。
    // 6. 调用 `core::Runtime::runtime_singleton_->RegisterAgent(new_agent)` 将其注册到运行时。
    
    printf("vHSA: Simulating agent discovery...\n");
    // 示例：手动创建一个假的 Agent 用于测试
    // HsaNodeProperties node_props = {0};
    // node_props.NumFComputeCores = 20; // 假的属性
    // AMD::GpuAgent* virtual_agent = new AMD::GpuAgent(0, node_props);
    // core::Runtime::runtime_singleton_->RegisterAgent(virtual_agent);

    return HSA_STATUS_SUCCESS;
}

hsa_status_t VhsaDriver::ShutDown() {
    printf("vHSA: ShutDown() called.\n");
    return HSA_STATUS_SUCCESS;
}

// --- 以下是其他需要覆盖的虚函数，在原型阶段，我们先提供存根实现 ---

hsa_status_t VhsaDriver::QueryKernelModeDriver(core::DriverQuery query) {
    printf("vHSA: QueryKernelModeDriver() NOT IMPLEMENTED.\n");
    // 我们可以伪造一个版本号
    if (query == core::DriverQuery::GET_DRIVER_VERSION) {
        version_.KernelInterfaceMajorVersion = 1;
        version_.KernelInterfaceMinorVersion = 2;
        return HSA_STATUS_SUCCESS;
    }
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::GetSystemProperties(HsaSystemProperties& sys_props) const {
    printf("vHSA: GetSystemProperties() NOT IMPLEMENTED.\n");
    // TODO (vHSA): 从宿主机获取并转发真实信息
    sys_props.NumNodes = 1; // 至少伪造一个节点
    return HSA_STATUS_SUCCESS;
}

hsa_status_t VhsaDriver::GetNodeProperties(HsaNodeProperties& node_props, uint32_t node_id) const {
    printf("vHSA: GetNodeProperties() NOT IMPLEMENTED.\n");
    // TODO (vHSA): 从宿主机获取并转发真实信息
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::AllocateMemory(const core::MemoryRegion &mem_region,
                              core::MemoryRegion::AllocateFlags alloc_flags,
                              void **mem, size_t size,
                              uint32_t node_id) {
    printf("vHSA: AllocateMemory() NOT IMPLEMENTED.\n");
    // TODO (vHSA): 核心转发逻辑。将分配请求（大小，flags，节点）发送给QEMU后端，
    // 后端在宿主机上调用真实的 hsaKmtAllocMemory，然后将返回的指针（或其映射）传回来。
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::FreeMemory(void *mem, size_t size) {
    printf("vHSA: FreeMemory() NOT IMPLEMENTED.\n");
    // TODO (vHSA): 将释放请求转发给QEMU后端。
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::CreateQueue(core::Queue &queue) const {
    printf("vHSA: CreateQueue() NOT IMPLEMENTED.\n");
    // TODO (vHSA): 将队列创建请求转发给QEMU后端，获取真实的队列ID和门铃地址。
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::DestroyQueue(core::Queue &queue) const {
    printf("vHSA: DestroyQueue() NOT IMPLEMENTED.\n");
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}


// --- 其他所有函数的简单存根实现 ---

hsa_status_t VhsaDriver::GetEdgeProperties(std::vector<HsaIoLinkProperties>&, uint32_t) const { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::GetAgentProperties(core::Agent&) const { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::GetMemoryProperties(uint32_t, std::vector<HsaMemoryProperties>&) const { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::GetCacheProperties(uint32_t, uint32_t, std::vector<HsaCacheProperties>&) const { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::ExportDMABuf(void*, size_t, int*, size_t*) { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::ImportDMABuf(int, core::Agent&, core::ShareableHandle&) { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::Map(core::ShareableHandle, void*, size_t, size_t, hsa_access_permission_t) { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::Unmap(core::ShareableHandle, void*, size_t, size_t) { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::ReleaseShareableHandle(core::ShareableHandle&) { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::SPMAcquire(uint32_t) const { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::SPMRelease(uint32_t) const { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::SPMSetDestBuffer(uint32_t, uint32_t, uint32_t*, uint32_t*, void*, bool*) const { return HSA_STATUS_ERROR_NOT_IMPLEMENTED; }
hsa_status_t VhsaDriver::IsModelEnabled(bool* enable) const { *enable=false; return HSA_STATUS_SUCCESS; }


} // namespace AMD
} // namespace rocr