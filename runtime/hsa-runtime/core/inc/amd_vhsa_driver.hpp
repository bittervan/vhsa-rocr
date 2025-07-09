#ifndef HSA_RUNTIME_CORE_INC_AMD_VHSA_DRIVER_H_
#define HSA_RUNTIME_CORE_INC_AMD_VHSA_DRIVER_H_

#include <memory>
#include <string>
#include <vector>

// 核心依赖：我们需要继承自 core::Driver
#include "core/inc/driver.h"
#include "hsa.h"

// 同样，把我们的实现也放在 rocr 命名空间下，创建一个新的子命名空间
namespace rocr {

namespace AMD {

/// @brief vHSA 虚拟设备驱动
///
/// @details 这是与 QEMU 中 vHSA 虚拟设备进行通信的用户态驱动。
/// 它实现了 core::Driver 接口，并将所有请求转发到宿主机（Host）执行。
class VhsaDriver final : public core::Driver {
public:
    VhsaDriver(std::string devnode_name);
    // 虚析构函数是良好实践
    virtual ~VhsaDriver();

    // --- 实现 core::Driver 的所有虚函数 ---
    // 我们必须覆盖所有父类的纯虚函数

    static hsa_status_t DiscoverDriver(std::unique_ptr<core::Driver>& driver);

    /// @brief 初始化 vHSA 驱动，与 QEMU 后端建立连接
    hsa_status_t Init() override;

    /// @brief 关闭与 QEMU 后端的连接
    hsa_status_t ShutDown() override;
    
    // 以下函数暂时可以返回“不支持”或空实现，我们后续再逐一攻克
    
    hsa_status_t QueryKernelModeDriver(core::DriverQuery query) override;
    hsa_status_t Open() override;
    hsa_status_t Close() override;
    hsa_status_t GetSystemProperties(HsaSystemProperties& sys_props) const override;
    hsa_status_t GetNodeProperties(HsaNodeProperties& node_props, uint32_t node_id) const override;
    hsa_status_t GetEdgeProperties(std::vector<HsaIoLinkProperties>& io_link_props,
                                   uint32_t node_id) const override;
    hsa_status_t GetAgentProperties(core::Agent &agent) const override;
    hsa_status_t GetMemoryProperties(uint32_t node_id,
                                     std::vector<HsaMemoryProperties>& mem_props) const override;
    hsa_status_t GetCacheProperties(uint32_t node_id, uint32_t processor_id,
                                    std::vector<HsaCacheProperties>& cache_props) const override;
    
    /// @brief 内存分配的核心转发函数
    hsa_status_t AllocateMemory(const core::MemoryRegion &mem_region,
                                core::MemoryRegion::AllocateFlags alloc_flags,
                                void **mem, size_t size,
                                uint32_t node_id) override;
    
    hsa_status_t FreeMemory(void *mem, size_t size) override;

    /// @brief 队列创建的核心转发函数
    hsa_status_t CreateQueue(core::Queue &queue) const override;
    hsa_status_t DestroyQueue(core::Queue &queue) const override;

    // DMA-Buf 和 SPM 相关功能在原型阶段可以暂时不实现
    hsa_status_t ExportDMABuf(void *mem, size_t size, int *dmabuf_fd,
                              size_t *offset) override;
    hsa_status_t ImportDMABuf(int dmabuf_fd, core::Agent &agent,
                              core::ShareableHandle &handle) override;
    hsa_status_t Map(core::ShareableHandle handle, void *mem, size_t offset,
                       size_t size, hsa_access_permission_t perms) override;
    hsa_status_t Unmap(core::ShareableHandle handle, void *mem, size_t offset,
                       size_t size) override;
    hsa_status_t ReleaseShareableHandle(core::ShareableHandle &handle) override;
    hsa_status_t SPMAcquire(uint32_t preferred_node_id) const override;
    hsa_status_t SPMRelease(uint32_t preferred_node_id) const override;
    hsa_status_t SPMSetDestBuffer(uint32_t preferred_node_id, uint32_t size_bytes, uint32_t* timeout,
                                  uint32_t* size_copied, void* dest_mem_addr,
                                  bool* is_spm_data_loss) const override;

    hsa_status_t IsModelEnabled(bool* enable) const override;

private:
    // TODO (vHSA): 在这里定义与 QEMU 后端通信所需的私有成员
    // 比如：
    // int qemu_device_fd_;       // mmap /dev/mem 或 /dev/uioX 的文件描述符
    // void* qemu_device_mmap_; // 映射后的 MMIO 地址
};

} // namespace VHSA
} // namespace rocr


// === 这是我们之前讨论的、需要在 ROCR 运行时中注册的“发现函数” ===
// 它的声明可以放在这里，或者一个更全局的头文件中

#endif // HSA_RUNTIME_CORE_INC_AMD_VHSA_DRIVER_H_