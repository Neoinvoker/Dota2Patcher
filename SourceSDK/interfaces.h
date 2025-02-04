#pragma once
class CDOTAGamerules;  // Forward declaration
#include "CEngineClient.h"
#include "CSource2Client.h"
#include "CDOTACamera.h"
#include "CGameEntitySystem.h"

class Scanner {
public:
    static bool find_CDOTACamera();
    static bool find_CDOTAGamerules();
    static bool find_CGameEntitySystem();
    static bool find_all();
};

static class dota_interfaces {
public:
    bool find_all() {
        return Scanner::find_all();
    }

    CEngineClient* engine;
    CSource2Client* client;
    CDOTAGamerules* gamerules;
    CDOTACamera* camera;
    CGameEntitySystem* entity_system;
} vmt;

bool Scanner::find_CDOTACamera() {
// CDOTA_Camera 20'th vfunc (offset 0xA0):
// sub                          rsp, 48h
// movaps[rsp + 48h + var_18],  xmm6
// movaps                       xmm6, xmm1
// movaps[rsp + 48h + var_28],  xmm7
// call                         get_camera_distance <<<
// 
// get_camera_distance:
// mov                          edx, cs:TlsIndex
// mov                          rax, gs:58h
// mov                          ecx, 40h ; '@'
// mov                          rax, [rax+rdx*8]
// mov                          eax, [rcx+rax]
// cmp                          cs:dword_184FE0578, eax
// jg                           short loc_1816542DF
// lea                          rax, dword_184FE0560 <<<<<
// add                          rsp, 20h
// pop                          rbx
// retn

    const auto base = Memory::pattern_scan("client.dll", Patches::Patterns::CDOTACamera);
    if (!base)
        return false;

    const auto camera_base_address = Memory::absolute_address<uintptr_t>(base.value());
    if (!camera_base_address)
        return false;

    printf("[+] CDOTA_Camera -> [%p]\n", reinterpret_cast<void*>(camera_base_address.value() - 0x40));
    vmt.camera = (CDOTACamera*)camera_base_address.value() - 0x40;
    return true;
}

bool Scanner::find_CDOTAGamerules() {
    const auto base = Memory::pattern_scan("client.dll", Patches::Patterns::CDOTAGamerules);
    if (!base)
        return false;
    
    const auto gamerules_proxy = Memory::absolute_address<uintptr_t>(base.value());
    if (!gamerules_proxy)
        return false;

    // Not really C_DOTAGamerules_Proxy but who cares
    printf("[+] C_DOTAGamerules_Proxy -> [%p]\n", reinterpret_cast<void*>(gamerules_proxy.value()));
    vmt.gamerules = (CDOTAGamerules*)gamerules_proxy.value();
    return true;
}

bool Scanner::find_CGameEntitySystem() {
    const auto base = Memory::virtual_function<uintptr_t>(vmt.client, 30);
    if (!base)
        return false;

    const auto absolute_address_ptr = Memory::absolute_address<uintptr_t>(base.value());
    if (!absolute_address_ptr)
        return false;

    const auto CGameEntitySystem_ptr = Memory::read_memory<uintptr_t>(absolute_address_ptr.value());
    if (!CGameEntitySystem_ptr)
        return false;

    printf("[+] CGameEntitySystem -> [%p]\n", reinterpret_cast<void*>(CGameEntitySystem_ptr.value()));
    vmt.entity_system = (CGameEntitySystem*)CGameEntitySystem_ptr.value();
    return true;
}

bool Scanner::find_all() {
    bool status = true;

    status &= find_CDOTACamera();
    status &= find_CDOTAGamerules();
    status &= find_CGameEntitySystem();

    return status;
}
