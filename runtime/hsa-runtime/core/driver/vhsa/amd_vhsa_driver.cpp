#include "core/inc/amd_vhsa_driver.hpp"

#include "core/inc/driver.h"
#include "core/inc/memory_region.h"
#include "hsa.h"
#include "hsakmt/hsakmttypes.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <dirent.h>
#include <cstring>
#include <string>

#include "core/inc/amd_kfd_driver.h"

#include <memory>
#include <string>

#include <amdgpu_drm.h>
#include <link.h>
#include <sys/ioctl.h>

#include "hsakmt/hsakmt.h"

#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/runtime.h"

extern r_debug _amdgpu_r_debug;

namespace rocr {
namespace AMD {

hsa_status_t AMD::VhsaDriver::DiscoverDriver(std::unique_ptr<core::Driver>& driver) {
    auto tmp_driver = std::unique_ptr<core::Driver>(new VhsaDriver("/dev/vhsa"));

    if (tmp_driver->Open() == HSA_STATUS_SUCCESS) {
        driver = std::move(tmp_driver);
        return HSA_STATUS_SUCCESS;
    }

    return HSA_STATUS_ERROR;
}

VhsaDriver::VhsaDriver(std::string devnode_name)
    : core::Driver(core::DriverType::VHSA, devnode_name) 
{
    printf("vHSA: VhsaDriver constructor called for device path: %s\n", devnode_name.c_str());
}

VhsaDriver::~VhsaDriver() {
    if (is_open_) {
        Close();
    }
}


HSAKMT_STATUS vhsaKmtOpenKFD(int fd) {
    HSAKMT_STATUS ret = (HSAKMT_STATUS)ioctl(fd, VHSA_REQ_OPEN_KFD, nullptr);
    printf("vHSA: vhsaKmtOpenKFD() returned %d\n", ret);
    return ret;
}

hsa_status_t VhsaDriver::Open() {
    fd_ = open(devnode_name_.c_str(), O_RDWR | O_CLOEXEC);
  return vhsaKmtOpenKFD(fd_) == HSAKMT_STATUS_SUCCESS ? HSA_STATUS_SUCCESS
                                                  : HSA_STATUS_ERROR;

}

hsa_status_t VhsaDriver::Close() {
    printf("vHSA: Close() called\n");
    int ret(0);
    if (fd_ > 0) {
        ret = close(fd_);
        fd_ = -1;
    }
    if (ret) {
        printf("vHSA: Close() failed\n");
        return HSA_STATUS_ERROR;
    }
    printf("vHSA: Close() successed\n");
    return HSA_STATUS_SUCCESS;
}

HSAKMT_STATUS vhsaKmtRuntimeEnable(int fd, bool setupTtmp)
{
    // Its impossilble for the host and guest to share a same r_debug structure
    // So we just ignore the rDebug parameter here
    struct vhsa_ioctl_vec* ioctl_vec = alloc_vhsa_ioctl_vec(0, 1);
    ioctl_vec->out_lens[0] = sizeof(bool);
    ioctl_vec->outs[0] = &setupTtmp;
    int ret = ioctl(fd, VHSA_REQ_RUNTIME_ENABLE, ioctl_vec);
    free_vhsa_ioctl_vec(ioctl_vec);
    return (HSAKMT_STATUS)ret;
}

HSAKMT_STATUS vhsaKmtGetRuntimeCapabilities(int fd, uint32_t* caps_mask)
{
    struct vhsa_ioctl_vec* ioctl_vec = alloc_vhsa_ioctl_vec(1, 0);
    ioctl_vec->in_lens[0] = sizeof(uint32_t);
    ioctl_vec->ins[0] = caps_mask;
    int ret = ioctl(fd, VHSA_REQ_GET_RUNTIME_CAPABILITIES, ioctl_vec);
    free_vhsa_ioctl_vec(ioctl_vec);
    return (HSAKMT_STATUS)ret;
}

HSAKMT_STATUS vhsaKmtGetVersion(int fd, HsaVersionInfo* version)
{
    struct vhsa_ioctl_vec* ioctl_vec = alloc_vhsa_ioctl_vec(1, 0);
    ioctl_vec->in_lens[0] = sizeof(HsaVersionInfo);
    ioctl_vec->ins[0] = version;
    int ret = ioctl(fd, VHSA_REQ_GET_VERSION, ioctl_vec);
    free_vhsa_ioctl_vec(ioctl_vec);
    return (HSAKMT_STATUS)ret;
}

hsa_status_t VhsaDriver::Init() {
    HSAKMT_STATUS ret =
        vhsaKmtRuntimeEnable(fd_, core::Runtime::runtime_singleton_->flag().debug());

    if (ret != HSAKMT_STATUS_SUCCESS && ret != HSAKMT_STATUS_NOT_SUPPORTED) return HSA_STATUS_ERROR;

    uint32_t caps_mask = 0;
    if (vhsaKmtGetRuntimeCapabilities(fd_, &caps_mask) != HSAKMT_STATUS_SUCCESS) return HSA_STATUS_ERROR;

    core::Runtime::runtime_singleton_->KfdVersion(
        ret != HSAKMT_STATUS_NOT_SUPPORTED,
        !!(caps_mask & HSA_RUNTIME_ENABLE_CAPS_SUPPORTS_CORE_DUMP_MASK));

    if (vhsaKmtGetVersion(fd_, &version_) != HSAKMT_STATUS_SUCCESS) return HSA_STATUS_ERROR;

    if (version_.KernelInterfaceMajorVersion == kfd_version_major_min &&
        version_.KernelInterfaceMinorVersion < kfd_version_major_min)
        return HSA_STATUS_ERROR;

    core::Runtime::runtime_singleton_->KfdVersion(version_);

    if (version_.KernelInterfaceMajorVersion == 1 && version_.KernelInterfaceMinorVersion == 0)
        core::g_use_interrupt_wait = false;

    bool xnack_mode = BindXnackMode();
    core::Runtime::runtime_singleton_->XnackEnabled(xnack_mode);

    return HSA_STATUS_SUCCESS;
}

hsa_status_t VhsaDriver::ShutDown() {
    printf("vHSA: ShutDown() called.\n");
    return HSA_STATUS_SUCCESS;
}

hsa_status_t VhsaDriver::QueryKernelModeDriver(core::DriverQuery query) {
    printf("vHSA: QueryKernelModeDriver() NOT IMPLEMENTED.\n");
    if (query == core::DriverQuery::GET_DRIVER_VERSION) {
        version_.KernelInterfaceMajorVersion = 1;
        version_.KernelInterfaceMinorVersion = 2;
        return HSA_STATUS_SUCCESS;
    }
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

HSAKMT_STATUS vhsaKmtReleaseSystemProperties(int fd)
{
    ioctl(fd, VHSA_REQ_RELEASE_SYSTEM_PROPERTIES, nullptr);

    return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS vhsaKmtAcquireSystemProperties(int fd, HsaSystemProperties *SystemProperties)
{
    vhsa_ioctl_vec* ioctl_vec = alloc_vhsa_ioctl_vec(1, 0);
    ioctl_vec->in_lens[0] = sizeof(HsaSystemProperties);
    ioctl_vec->ins[0] = SystemProperties;
    int ret = ioctl(fd, VHSA_REQ_ACQUIRE_SYSTEM_PROPERTIES, ioctl_vec);
    free_vhsa_ioctl_vec(ioctl_vec);
    return (HSAKMT_STATUS)ret;
}


hsa_status_t VhsaDriver::GetSystemProperties(HsaSystemProperties& sys_props) const {
    printf("vHSA: GetSystemProperties() called.\n");

    if (vhsaKmtReleaseSystemProperties(fd_) != HSAKMT_STATUS_SUCCESS) return HSA_STATUS_ERROR;
   
    if (vhsaKmtAcquireSystemProperties(fd_, &sys_props) != HSAKMT_STATUS_SUCCESS) return HSA_STATUS_ERROR;

    return HSA_STATUS_SUCCESS;
}

hsa_status_t VhsaDriver::GetNodeProperties(HsaNodeProperties& node_props, uint32_t node_id) const {
    printf("vHSA: GetNodeProperties() called.\n");

    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::AllocateMemory(const core::MemoryRegion &mem_region,
                              core::MemoryRegion::AllocateFlags alloc_flags,
                              void **mem, size_t size,
                              uint32_t node_id) {
    printf("vHSA: AllocateMemory() NOT IMPLEMENTED.\n");
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::FreeMemory(void *mem, size_t size) {
    printf("vHSA: FreeMemory() NOT IMPLEMENTED.\n");
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::CreateQueue(core::Queue &queue) const {
    printf("vHSA: CreateQueue() NOT IMPLEMENTED.\n");
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

hsa_status_t VhsaDriver::DestroyQueue(core::Queue &queue) const {
    printf("vHSA: DestroyQueue() NOT IMPLEMENTED.\n");
    return HSA_STATUS_ERROR_NOT_IMPLEMENTED;
}

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
// hsa_status_t VhsaDriver::IsModelEnabled(bool* enable) const { *enable=false; return HSA_STATUS_SUCCESS; }

HSAKMT_STATUS vhsaKmtSetXNACKMode(int fd, HSAint32 enable)
{
    vhsa_ioctl_vec* ioctl_vec = alloc_vhsa_ioctl_vec(0, 1);
    ioctl_vec->out_lens[0] = sizeof(HSAint32);
    ioctl_vec->outs[0] = &enable;
    int ret = ioctl(fd, VHSA_REQ_SET_XNACK_MODE, ioctl_vec);
    free_vhsa_ioctl_vec(ioctl_vec);
    return (HSAKMT_STATUS)ret;
}

HSAKMT_STATUS vhsaKmtGetXNACKMode(int fd, HSAint32 * enable)
{
    vhsa_ioctl_vec* ioctl_vec = alloc_vhsa_ioctl_vec(1, 0);
    ioctl_vec->in_lens[0] = sizeof(HSAint32);
    ioctl_vec->ins[0] = enable;
    int ret = ioctl(fd, VHSA_REQ_GET_XNACK_MODE, ioctl_vec);
    free_vhsa_ioctl_vec(ioctl_vec);
    return (HSAKMT_STATUS)ret;
}

HSAKMT_STATUS vhsaKmtModelEnabled(int fd, bool* enable)
{
    vhsa_ioctl_vec* ioctl_vec = alloc_vhsa_ioctl_vec(1, 0);
    ioctl_vec->in_lens[0] = sizeof(bool);
    ioctl_vec->ins[0] = enable;
    int ret = ioctl(fd, VHSA_REQ_MODEL_ENABLED, ioctl_vec);
    free_vhsa_ioctl_vec(ioctl_vec);
    return (HSAKMT_STATUS)ret;
    //return (HSAKMT_STATUS)
}

bool VhsaDriver::BindXnackMode() {
  // Get users' preference for Xnack mode of ROCm platform.
  HSAint32 mode = core::Runtime::runtime_singleton_->flag().xnack();
  bool config_xnack = (mode != Flag::XNACK_REQUEST::XNACK_UNCHANGED);

  // Indicate to driver users' preference for Xnack mode
  // Call to driver can fail and is a supported feature
  HSAKMT_STATUS status = HSAKMT_STATUS_ERROR;
  if (config_xnack) {
    status = vhsaKmtSetXNACKMode(fd_, mode);
    if (status == HSAKMT_STATUS_SUCCESS) {
      return (mode != Flag::XNACK_DISABLE);
    }
  }

  // Get Xnack mode of devices bound by driver. This could happen
  // when a call to SET Xnack mode fails or user has no particular
  // preference
  status = vhsaKmtGetXNACKMode(fd_, &mode);
  if (status != HSAKMT_STATUS_SUCCESS) {
    debug_print(
        "KFD does not support xnack mode query.\nROCr must assume "
        "xnack is disabled.\n");
    return false;
  }
  return (mode != Flag::XNACK_DISABLE);
}

hsa_status_t VhsaDriver::IsModelEnabled(bool* enable) const {
  // AIE does not support streaming performance monitor.
  HSAKMT_STATUS status = HSAKMT_STATUS_ERROR;
  status = vhsaKmtModelEnabled(fd_, enable);
  if (status != HSAKMT_STATUS_SUCCESS)
     return HSA_STATUS_ERROR;

  return HSA_STATUS_SUCCESS;
}

struct vhsa_ioctl_vec* alloc_vhsa_ioctl_vec(uint32_t num_ins, uint32_t num_outs) {
    struct vhsa_ioctl_vec *ret = (struct vhsa_ioctl_vec*)malloc(sizeof(struct vhsa_ioctl_vec));
    ret->num_ins = num_ins;
    ret->ins = (void**)malloc(num_ins * sizeof(void*));
    ret->in_lens = (uint32_t*)malloc(num_ins * sizeof(uint32_t));
    ret->num_outs = num_outs;
    ret->outs = (void**)malloc(num_outs * sizeof(void*));
    ret->out_lens = (uint32_t*)malloc(num_outs * sizeof(uint32_t));
    return ret;
}

void free_vhsa_ioctl_vec(struct vhsa_ioctl_vec *ptr) {
    free(ptr->ins);
    free(ptr->in_lens);
    free(ptr->outs);
    free(ptr->out_lens);
    free(ptr);
}
} // namespace AMD
} // namespace rocr