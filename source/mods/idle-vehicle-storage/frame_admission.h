#ifndef MOON_IDLE_FRAME_ADMISSION_H
#define MOON_IDLE_FRAME_ADMISSION_H
/* Unknown identity invalidates idle evidence, never an outstanding native
 * transaction. Positively resolved peers cannot mutate the local scan state. */
static int idle_frame_admit(PTR walker,PTR *world){
 PTR local=0;*world=0;
 if(!moon_local_actor_read(&local)||!local){clear_tracking();return 0;}
 if(local!=walker)return 0;
 int ok=1;field_value(walker,world_f,world);
 if(!alive(*world)||!host()||vehicle_read_boolean(restoring_m,*world,&ok)||!ok){clear_tracking();return 0;}
 /* Only a confirmed different local world discards old-world transaction state.
  * Temporary identity, authority or rendering loss preserves that state. */
 if(dget(tracked_world)!=*world){reset();tracked_world=dhold(*world);if(!tracked_world)return 0;}
 return 1;
}
#endif
