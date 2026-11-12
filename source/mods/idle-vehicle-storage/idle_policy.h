#ifndef MOON_IDLE_STORAGE_POLICY_H
#define MOON_IDLE_STORAGE_POLICY_H
/* Only consecutive observations count. A long scan gap, clock reversal, or
 * representation/identity change starts a new interval. */
typedef struct { float since,last; int observed,mode,eligible; } MoonIdleTimer;
static inline int moon_idle_time(float now) {
 return now>=0&&now<=3.402823466e+38F;
}
static inline int moon_idle_observe(MoonIdleTimer *t,float now,int eligible,
                                    int mode,int changed,int timeout) {
 if(!t)return 0;
 if(!moon_idle_time(now)||timeout<30||timeout>86400){t->observed=t->eligible=0;return 0;}
 if(!t->observed||!t->eligible||now<t->last||now-t->last>4.5f||t->mode!=mode||changed){
  t->since=now;t->observed=1;
 }
 t->last=now;t->mode=mode;t->eligible=eligible;
 if(!eligible)t->since=now;
 return eligible&&now-t->since>=(float)timeout;
}
#endif
