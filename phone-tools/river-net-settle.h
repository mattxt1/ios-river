/* Bounded post-acquisition reconciliation; host-tested independently.
 * A missing native primary network does not count as missing service state.
 * Never repeat an acquisition, force interface rank, or alter DNS settings. */
struct river_net_settle {unsigned checks,republishes;time_t next;};
static void river_net_settle_reset(struct river_net_settle *s,time_t now)
{memset(s,0,sizeof(*s));s->next=now+2;}
static int river_net_settle_due(struct river_net_settle *s,time_t now,int associated)
{return associated && s->checks<8 && now>=s->next;}
static int river_net_settle_observed(struct river_net_settle *s,time_t now,int state)
{
 s->checks++;s->next=now+2;
 /* state<0 means unreadable: never turn a failed query into a write. */
 if(state!=0 || s->republishes>=3)return 0;
 s->republishes++;return 1;
}
