/* Host-authoritative conservative idle recall through the game's garage transaction. */
#include "base.h"
#include "../vehicle-framework/sdk/fleet_adapter.h"
static PTR containers_m,world_dust_m,null_m,speed_m,driven_m,auto_m,clean_m,cargo_m,dust_m,recall_m,save_m,velocity_m,angular_m;
static DRoot tracked_world;static float next_scan;static int timeout=300;
static struct {DRoot vehicle;float since;unsigned seen;DVec position;} cars[256];static unsigned generation;
static float norm(DVec v){return v.x*v.x+v.y*v.y+v.z*v.z;}
static void reset(void){for(int i=0;i<256;i++)ddrop(&cars[i].vehicle);ddrop(&tracked_world);}
static int idle(PTR v){int ok=1;PTR body=0;field_value(v,body_f,&body);if(!body)return 0;float speed=dnumber(speed_m,v,&ok);PTR dust=call(dust_m,v,0,&ok);DVec vel=dvector(velocity_m,body,&ok),spin=dvector(angular_m,body,&ok);
 return ok&&dust&&*(double*)unbox(dust)<.0001&&integer(cargo_m,v,&ok)==0&&!boolean(driven_m,v,&ok)&&!boolean(auto_m,v,&ok)&&!boolean(clean_m,v,&ok)&&speed>-.1f&&speed<.1f&&norm(vel)<.01f&&norm(spin)<.01f&&ok;
}
static int before(int id,Context *c){(void)id;if(!on_thread())return 0;int ok=1;PTR world=0;field_value((PTR)c->rcx,world_f,&world);float now=dnumber(time_m,0,&ok);if(!alive(world)||!host()||boolean(restoring_m,world,&ok)||!ok){reset();return 0;}if(dget(tracked_world)!=world){reset();tracked_world=dhold(world);}if(now<next_scan)return 0;next_scan=now+2;generation++;
 MoonFleetSnapshot snapshot={0};if(!mf_snapshot(world,&snapshot)){event("vehicle-scan-limit");return 0;}
 for(unsigned i=0;i<snapshot.count;i++){PTR owned=mf_item(&snapshot,i);MoonFleetRecord record={0};if(!mf_read(owned,&record))continue;
 PTR cls=object_class(owned),pos_f=get_field(cls,"position");if(!pos_f)continue;int inworld=record.deployed;PTR holder=record.holder,key_obj=record.storage;
 int slot=-1;for(int j=0;j<256;j++)if(dget(cars[j].vehicle)==owned){slot=j;break;}if(slot<0)for(int j=0;j<256;j++)if(!cars[j].vehicle){slot=j;cars[j].vehicle=dhold(owned);cars[j].since=now;break;}if(slot<0)continue;cars[slot].seen=generation;
 PTR live=mf_live(world,key_obj,&ok);int unused=0;if(alive(live))unused=idle(live);else{void *ha[]={holder};PTR empty=call(null_m,0,ha,&ok);void *ka[]={key_obj};PTR dust=call(world_dust_m,world,ka,&ok),cargo=call(containers_m,world,ka,&ok);int cargo_count=cargo?integer(get_method(object_class(cargo),"get_Count",0),cargo,&ok):0;DVec pos={0};field_value(owned,pos_f,&pos);DVec delta={pos.x-cars[slot].position.x,pos.y-cars[slot].position.y,pos.z-cars[slot].position.z};cars[slot].position=pos;unused=inworld&&empty&&*(unsigned char*)unbox(empty)&&dust&&*(double*)unbox(dust)<.0001&&cargo_count==0&&norm(delta)<.01f&&ok;}
 if(unused&&!moon_fleet_unoccupied(&mf_queries,&record))unused=0;
 if(!unused){cars[slot].since=now;continue;}if(now-cars[slot].since<(float)timeout)continue;PTR message=0;void *r[]={key_obj,&message};PTR result=call(recall_m,world,r,&ok);if(ok&&result&&*(unsigned char*)unbox(result)){call(save_m,world,0,&ok);event("idle-vehicle-stored");}cars[slot].since=now;
 }mf_close(&snapshot);for(int j=0;j<256;j++)if(cars[j].vehicle&&cars[j].seen!=generation)ddrop(&cars[j].vehicle);return 0;
}
int MoonLab_ModStart(PTR domain,PTR game){if(!common(domain,game,"idle-vehicle-storage.jsonl")||!mf_bind(game,1))return 0;
 char path[1100];DWORD n=GetEnvironmentVariableA("MOONLAB_IDLE_VEHICLE_STORAGE_CONFIG",path,sizeof(path));if(!n||n>=sizeof(path))cat(path,".\\MoonLab\\mods\\idle-vehicle-storage\\settings.ini");DWORD(__stdcall *ini)(const char*,const char*,int,const char*)=(DWORD(__stdcall *)(const char*,const char*,int,const char*))GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetPrivateProfileIntA");if(!ini)return 0;timeout=(int)ini("idle-storage","timeout_seconds",300,path);if(timeout<30||timeout>86400)timeout=300;
#define M(F,NS,T,N,S) F=dmethod(NS,T,N,S);if(!F)return 0
 M(containers_m,"MoonCleaners","MoonWorld","StoredContainers","System.String");M(world_dust_m,"MoonCleaners","MoonWorld","VehicleDustKilograms","System.String");M(null_m,"System","String","IsNullOrEmpty","System.String");M(speed_m,"MoonCleaners","MoonPrototypeVehicle","get_Speed","");M(driven_m,"MoonCleaners","MoonPrototypeVehicle","get_IsDriven","");M(auto_m,"MoonCleaners","MoonPrototypeVehicle","get_Autopilot","");M(clean_m,"MoonCleaners","MoonPrototypeVehicle","get_Cleaning","");M(cargo_m,"MoonCleaners","MoonPrototypeVehicle","get_CargoCount","");M(dust_m,"MoonCleaners","MoonPrototypeVehicle","get_DustKilograms","");M(recall_m,"MoonCleaners","MoonWorld","RecallToGarage","System.String;System.String&");M(save_m,"MoonCleaners","MoonWorld","RequestSave","");M(velocity_m,"UnityEngine","Rigidbody","get_linearVelocity","");M(angular_m,"UnityEngine","Rigidbody","get_angularVelocity","");
#undef M
 if(!update_hook(game))return 0;before_hook=before;event("idle-storage-ready");return install_hooks();}
