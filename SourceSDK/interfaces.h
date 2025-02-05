#pragma once
#include "CEngineClient.h"
#include "CSource2Client.h"
#include "CDOTAGamerules.h"
#include "CDOTACamera.h"
#include "CGameEntitySystem.h"

class Scanner {
public:
    static bool find_CGameEntitySystem();
    static bool find_CDOTACamera();
    static bool find_CDOTAGamerules();
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


bool Scanner::find_all() {
    bool status = true;

    status &= find_CGameEntitySystem();
    status &= find_CDOTACamera();
    status &= find_CDOTAGamerules();

    return status;
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

    vmt.entity_system = reinterpret_cast<CGameEntitySystem*>(CGameEntitySystem_ptr.value());
    printf("[+] CGameEntitySystem -> [%p]\n", (void*)vmt.entity_system);
    return true;
}

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

    vmt.camera = reinterpret_cast<CDOTACamera*>(camera_base_address.value() - 0x40);
    printf("[+] CDOTA_Camera -> [%p]\n", (void*)vmt.camera);
    return true;
}

bool Scanner::find_CDOTAGamerules() {
    printf("\n[~] Waiting for a lobby to start...\n");
    C_DOTAGamerulesProxy* dota_gamerules_proxy = nullptr;

    while (!dota_gamerules_proxy) {
        const auto dota_gamerules_proxy_ptr = vmt.entity_system->find_by_name("dota_gamerules");
        if (dota_gamerules_proxy_ptr) {    
            dota_gamerules_proxy = reinterpret_cast<C_DOTAGamerulesProxy*>(dota_gamerules_proxy_ptr.value());
            printf("[+] C_DOTAGamerules_Proxy -> [%p]\n", (void*)dota_gamerules_proxy_ptr.value());
            break;
        }
        Sleep(1000);
    }

    const auto dota_gamerules_ptr = dota_gamerules_proxy->gamerules();
    if (!dota_gamerules_ptr)
        return false;

    vmt.gamerules = reinterpret_cast<CDOTAGamerules*>(dota_gamerules_ptr.value());
    printf("[+] CDOTAGamerules -> [%p]\n", (void*)vmt.gamerules);
    return true;
}
