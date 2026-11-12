/* Host-authoritative conservative idle recall through the game's garage transaction. */
#include "base.h"
#include "../../runtime/native/local_actor.h"
#include "../vehicle-framework/sdk/fleet_adapter.h"
#include "idle_policy.h"
static PTR speed_m,driven_m,auto_m,clean_m,cargo_m,dust_m,recall_m,save_m,velocity_m,angular_m;
static DRoot tracked_world;static float next_scan,last_scan;static int timeout=300,scanned,save_pending,recall_uncertain;
static struct {DRoot vehicle,storage,buyer,holder;MoonIdleTimer timer;unsigned seen;} cars[256];
static unsigned generation;
static float norm(DVec v){return v.x*v.x+v.y*v.y+v.z*v.z;}
static void clear_car(int i){
 ddrop(&cars[i].vehicle);ddrop(&cars[i].storage);ddrop(&cars[i].buyer);ddrop(&cars[i].holder);
 dzero(&cars[i],sizeof(cars[i]));
}
static void clear_tracking(void){
 for(int i=0;i<256;i++)clear_car(i);
 next_scan=last_scan=0;scanned=0;generation=0;
}
static void reset(void){
 clear_tracking();ddrop(&tracked_world);save_pending=recall_uncertain=0;
}
static int idle(PTR v){
 int ok=1;PTR body=0;field_value(v,body_f,&body);if(!alive(body))return 0;
 float speed=vehicle_read_number(speed_m,v,&ok);PTR dust=call(dust_m,v,0,&ok);
 PTR mass=ok&&dust?unbox(dust):0;double kilograms=mass?*(double*)mass:-1;
 DVec vel=vehicle_read_vector(velocity_m,body,&ok),spin=vehicle_read_vector(angular_m,body,&ok);
 return ok&&kilograms>=0&&kilograms<.0001&&
  vehicle_read_integer(cargo_m,v,&ok)==0&&!vehicle_read_boolean(driven_m,v,&ok)&&!vehicle_read_boolean(auto_m,v,&ok)&&
  !vehicle_read_boolean(clean_m,v,&ok)&&speed>-.1f&&speed<.1f&&norm(vel)<.01f&&norm(spin)<.01f&&ok;
}
/* Retain identity independently: handover can replace fields on the same
 * owned record between scans. A failed root rejects the record. */
static int identity(int slot,const MoonFleetRecord *r,int *changed){
 *changed=!mf_equal(dget(cars[slot].storage),r->storage)||
          !mf_equal(dget(cars[slot].buyer),r->owner)||
          !mf_equal(dget(cars[slot].holder),r->holder);
 if(!*changed)return 1;
 DRoot storage=dhold(r->storage),buyer=dhold(r->owner),holder=dhold(r->holder);
 if(!storage||!buyer||(r->holder&&!holder)){
  ddrop(&storage);ddrop(&buyer);ddrop(&holder);return 0;
 }
 ddrop(&cars[slot].storage);ddrop(&cars[slot].buyer);ddrop(&cars[slot].holder);
 cars[slot].storage=storage;cars[slot].buyer=buyer;cars[slot].holder=holder;
 return 1;
}
static int request_save(PTR world){
 int ok=1;call(save_m,world,0,&ok);
 if(!ok){event("idle-save-request-failed");return 0;}
 event("idle-save-requested");return 1;
}
#include "frame_admission.h"
static int before(int id,Context *c){
 (void)id;if(!on_thread())return 0;int ok=1;PTR world=0;
 if(!idle_frame_admit((PTR)c->rcx,&world))return 0;
 float now=vehicle_read_number(time_m,0,&ok);
 float scan_until=0;
 if(!ok||!moon_idle_time(now)||!vehicle_read_deadline(now,2,&scan_until)){clear_tracking();return 0;}
 /* Clock faults invalidate idle evidence, not an outstanding native save or
  * uncertain transaction outcome in this same authoritative world. */
 if(scanned&&now<last_scan)clear_tracking();
 if(now<next_scan)return 0;next_scan=scan_until;last_scan=now;scanned=1;generation++;
 /* An exceptional recall may have changed native state before throwing.
  * Only a confirmed different local world clears that uncertainty. */
 if(recall_uncertain)return 0;
 /* Native recall has committed. Retry only RequestSave and admit no new
  * recalls meanwhile; replaying recall is not valid save compensation. */
 if(save_pending){save_pending=!request_save(world);for(int j=0;j<256;j++)clear_car(j);return 0;}
 MoonFleetSnapshot snapshot={0};
 if(!mf_snapshot(world,&snapshot)){event("vehicle-scan-unavailable");for(int j=0;j<256;j++)clear_car(j);return 0;}
 for(unsigned i=0;i<snapshot.count;i++){
  PTR owned=mf_item(&snapshot,i);MoonFleetRecord record={0};if(!mf_read(owned,&record))continue;
  if(!record.owner)continue;
  int slot=-1;
  for(int j=0;j<256;j++)if(dget(cars[j].vehicle)==owned){slot=j;break;}
  if(slot<0)for(int j=0;j<256;j++)if(!cars[j].vehicle){slot=j;clear_car(j);cars[j].vehicle=dhold(owned);break;}
  if(slot<0)continue;if(!cars[slot].vehicle)continue;
  int changed=0;if(!identity(slot,&record,&changed)){clear_car(slot);continue;}
  cars[slot].seen=generation;ok=1;
  PTR live=mf_live(world,record.storage,&ok);int mode=alive(live),unused=0;
  /* StoredContainers takes a player ID, not vehicle storage. Until a proven
   * parked-vehicle contents reader exists, only live vehicles can qualify. */
  if(ok&&record.deployed&&mode)unused=idle(live);
  if(!ok||!moon_fleet_unoccupied(&mf_queries,&record))unused=0;
  if(!moon_idle_observe(&cars[slot].timer,now,unused,mode,changed,timeout))continue;
  PTR message=0;void *r[]={record.storage,&message};PTR result=call(recall_m,world,r,&ok);
  cars[slot].timer.since=now;
  PTR decision=ok&&result?unbox(result):0;
  if(!decision){event("idle-recall-uncertain");recall_uncertain=1;break;}
  if(!*(unsigned char*)decision){event("idle-recall-refused");continue;}
  event("idle-vehicle-stored");save_pending=!request_save(world);
  clear_car(slot);if(save_pending)break;
 }
 mf_close(&snapshot);
 for(int j=0;j<256;j++)if(cars[j].vehicle&&(recall_uncertain||cars[j].seen!=generation))clear_car(j);
 return 0;
}
int MoonLab_ModStart(PTR domain,PTR game){if(!common(domain,game,"idle-vehicle-storage.jsonl"))return 0;
 if(!moon_local_actor_bind()){event("local-actor-binding-failed");return 0;}
 if(!mf_bind(game,1)){event("fleet-passenger-binding-failed");return 0;}
 char path[1100];DWORD n=GetEnvironmentVariableA("MOONLAB_IDLE_VEHICLE_STORAGE_CONFIG",path,sizeof(path));if(!n||n>=sizeof(path))cat(path,".\\MoonLab\\mods\\idle-vehicle-storage\\settings.ini");DWORD(__stdcall *ini)(const char*,const char*,int,const char*)=(DWORD(__stdcall *)(const char*,const char*,int,const char*))GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetPrivateProfileIntA");if(!ini)return 0;timeout=(int)ini("idle-storage","timeout_seconds",300,path);if(timeout<30||timeout>86400)timeout=300;
#define M(F,NS,T,N,S) F=dmethod(NS,T,N,S);if(!F)return 0
 M(speed_m,"MoonCleaners","MoonPrototypeVehicle","get_Speed","");M(driven_m,"MoonCleaners","MoonPrototypeVehicle","get_IsDriven","");M(auto_m,"MoonCleaners","MoonPrototypeVehicle","get_Autopilot","");M(clean_m,"MoonCleaners","MoonPrototypeVehicle","get_Cleaning","");M(cargo_m,"MoonCleaners","MoonPrototypeVehicle","get_CargoCount","");M(dust_m,"MoonCleaners","MoonPrototypeVehicle","get_DustKilograms","");M(recall_m,"MoonCleaners","MoonWorld","RecallToGarage","System.String;System.String&");M(save_m,"MoonCleaners","MoonWorld","RequestSave","");M(velocity_m,"UnityEngine","Rigidbody","get_linearVelocity","");M(angular_m,"UnityEngine","Rigidbody","get_angularVelocity","");
#undef M
 if(!update_hook(game))return 0;before_hook=before;if(!install_hooks())return 0;event("idle-storage-ready");return 1;}
