#ifndef HSA_RUNTIME_CORE_INC_AMD_VHSA_DRIVER_HPP_
#define HSA_RUNTIME_CORE_INC_AMD_VHSA_DRIVER_HPP_

#include <memory>
#include <string>
#include <vector>

// 核心依赖：我们需要继承自 core::Driver
#include "core/inc/driver.h"
#include "hsa.h"

// 修正：将命名空间统一为小写开头的 vHSA，以符合我们的约定
namespace rocr {
namespace AMD {
/// @brief vHSA 虚拟设备驱动
///
/// @details 这是与 QEMU 中 vHSA 虚拟设备进行通信的用户态驱动。
/// 它实现了 core::Driver 接口，并将所有请求转发到宿主机（Host）执行。
class VhsaDriver final : public core::Driver {
public:
    // 构造函数，传入我们发现的设备路径
    VhsaDriver(std::string dev_path);
    // 新增：添加虚析构函数声明，以进行正确的资源清理
    virtual ~VhsaDriver();


    static hsa_status_t DiscoverDriver(std::unique_ptr<core::Driver>& driver);

    // --- 实现 core::Driver 的所有虚函数 ---
    
    hsa_status_t Init() override;
    hsa_status_t ShutDown() override;
    hsa_status_t Open() override;
    hsa_status_t Close() override;

    // --- 其他函数的声明保持不变 ---
    hsa_status_t QueryKernelModeDriver(core::DriverQuery query) override;
    hsa_status_t GetSystemProperties(HsaSystemProperties& sys_props) const override;
    hsa_status_t GetNodeProperties(HsaNodeProperties& node_props, uint32_t node_id) const override;
    hsa_status_t GetEdgeProperties(std::vector<HsaIoLinkProperties>& io_link_props,
                                   uint32_t node_id) const override;
    hsa_status_t GetAgentProperties(core::Agent &agent) const override;
    hsa_status_t GetMemoryProperties(uint32_t node_id,
                                     std::vector<HsaMemoryProperties>& mem_props) const override;
    hsa_status_t GetCacheProperties(uint32_t node_id, uint32_t processor_id,
                                    std::vector<HsaCacheProperties>& cache_props) const override;
    hsa_status_t AllocateMemory(const core::MemoryRegion &mem_region,
                                core::MemoryRegion::AllocateFlags alloc_flags,
                                void **mem, size_t size,
                                uint32_t node_id) override;
    hsa_status_t FreeMemory(void *mem, size_t size) override;
    hsa_status_t CreateQueue(core::Queue &queue) const override;
    hsa_status_t DestroyQueue(core::Queue &queue) const override;
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
    std::string pci_path_;           // 保存设备的 sysfs 路径
    bool is_open_ = false;           // 跟踪 Open/Close 状态
    int dev_mem_fd_ = -1;            // 用于 mmap /dev/mem 的文件描述符
    void* mmio_base_ = nullptr;      // 映射后的 MMIO 基地址指针
    size_t mmio_size_ = 0;           // MMIO 区域的大小

    /// @brief Allocate agent accessible memory (system / local memory).
    void *AllocateKfdMemory(const HsaMemFlags &flags, uint32_t node_id,
                                    size_t size);

    /// @brief Free agent accessible memory (system / local memory).
    bool FreeKfdMemory(void *mem, size_t size);

    /// @brief Pin memory.
    bool MakeKfdMemoryResident(size_t num_node, const uint32_t *nodes,
                                        const void *mem, size_t size,
                                        uint64_t *alternate_va,
                                        HsaMemMapFlags map_flag);

    /// @brief Unpin memory.
    void MakeKfdMemoryUnresident(const void *mem);

    /// @brief Query for user preference and use that to determine Xnack mode
    /// of ROCm system. Return true if Xnack mode is ON or false if OFF. Xnack
    /// mode of a system is orthogonal to devices that do not support Xnack mode.
    /// It is legal for a system with Xnack ON to have devices that do not support
    /// Xnack functionality.
    bool BindXnackMode();

    // Minimum acceptable KFD version numbers.
    const uint32_t kfd_version_major_min = 0;
    const uint32_t kfd_version_minor_min = 99;
};

enum vhsa_req_type {
    VHSA_REQ_OPEN_KFD,
    VHSA_REQ_RUNTIME_ENABLE,
    VHSA_REQ_GET_RUNTIME_CAPABILITIES,
    VHSA_REQ_GET_VERSION,
    VHSA_REQ_SET_XNACK_MODE,
    VHSA_REQ_GET_XNACK_MODE,
    VHSA_REQ_MODEL_ENABLED,
    VHSA_REQ_ACQUIRE_SYSTEM_PROPERTIES,
    VHSA_REQ_RELEASE_SYSTEM_PROPERTIES,
};

struct vhsa_ioctl_vec {
    uint32_t num_data_bufs;
    uint32_t *data_buf_lens_user;
    void **data_bufs_user;
};

struct vhsa_ioctl_vec* alloc_vhsa_ioctl_vec(uint32_t len);
void free_vhsa_ioctl_vec(struct vhsa_ioctl_vec *ptr);



} // namespace vHSA
} // namespace rocr


#endif // HSA_RUNTIME_CORE_INC_AMD_VHSA_DRIVER_HPP_