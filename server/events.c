/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

events.c: Event processors
*/

#define LIMIT_ADD_EVENT 512

#include "../inc/server.h"
#include <stdio.h>
#include <string.h>

extern int EBptr;
extern cliinfo_t C[MAX_CLIENTS];
extern uint8_t EventBuffer[SIZE_EVENT_BUFFER];
extern int UserCount;
extern userinfo_t *UserInformations;

void AddEvent(uint8_t*, int, int);
ssize_t GetEventPacketSize(uint8_t*, int);

void SendPrivateChat(int cid, int destcid, char* chat) {
	if(!(-1 <= cid && cid <= MAX_CLIENTS) ) {
		printf("SendPrivateChat(): invalid cid passed.\n");
		return;
	}
	if(!(0 <= destcid && destcid <= MAX_CLIENTS) ) {
		printf("SendPrivateChat(): invalid destcid passed.\n");
		return;
	}
	uint8_t t[SIZE_NET_BUFFER];
	size_t cl = strlen(chat);
	ev_chat_t hdr = {
		.evtype = EV_CHAT,
		.dstcid = destcid
	};
	size_t evsiz = cl + sizeof(hdr) + 1;
	if(evsiz >= SIZE_NET_BUFFER) {
		printf("SendPrivateChat(): EV_CHAT packet overflow.\n");
		return;
	}
	memcpy(t, &hdr, sizeof(hdr) );
	CopyAsString(&t[sizeof(hdr)], chat, cl);
	AddEvent(t, evsiz, cid);
}

void SendJoinPacket(int cid) {
	uint8_t t[UNAME_SIZE + 1];
	if(!(0 <= cid && cid <= MAX_CLIENTS) ) {
		printf("SendJoinPacket(): invalid cid passed.\n");
		return;
	}
	int uid = C[cid].uid;
	if(!(0 <= uid && uid <= UserCount) ) {
		printf("SetupJoinPacket(): good cid, but not logged in.\n");
		return;
	}
	int uname_len = strlen(UserInformations[uid].usr);
	t[0] = EV_HELLO;
	CopyAsString(&t[1], UserInformations[uid].usr, uname_len);
	AddEvent(t, uname_len + 2, cid);
}

void SendLeavePacket(int cid) {
	uint8_t ev[1] = { EV_BYE };
	if(!(0 <= cid && cid <= MAX_CLIENTS) ) {
		printf("SendLeavePacket(): invalid cid passed.\n");
		return;
	}
	//Do not execute for non logged in user
	if(C[cid].uid == -1) {
		return;
	}
	AddEvent(ev, 1, cid);
}

void AddEventCmd(uint8_t* d, int dlen, int cid) {
	if(!(0 <= cid && cid <= MAX_CLIENTS) ) {
		printf("AddEvent(): Passing bad cid\n");
		return;
	}
	if(!(0 <= C[cid].uid && C[cid].uid <= UserCount) ) {
		Log(cid, "AddEvent: Login first.\n");
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	if(1 <= dlen && dlen <= LIMIT_ADD_EVENT) {
		AddEvent(d, dlen, cid);
	//	Log(cid, "AddEvent: Event Added, Head:%d\n", EBptr);
	} else {
		Log(cid, "AddEvent: Too long event. (%d)\n", dlen);
		DisconnectWithReason(cid, "Too long event.");
	}
}

void AddEvent(uint8_t* d, int dlen, int cid) {
	//Check entire event size
	if(EBptr + dlen + sizeof(event_hdr_t) >= SIZE_EVENT_BUFFER) {
		Log(cid, "AddEvent: EventBuffer overflow.\n");
		return;
	}
	int evcp = 0;
	int pdlen = 0;
	while(evcp < dlen) {
		//Get event length
		ssize_t evlen;
		uint8_t *evhead = &d[evcp];
		evlen = GetEventPacketSize(evhead, dlen);
		if(evlen == -1) {
			Log(cid, "AddEvent: Bad packet. Disconnecting client.\n");
			if(cid != -1) {
				DisconnectWithReason(cid, "Bad event packet.");
			}
			return;
		}
		
		//Process events
		int copyev = 1;
		if(evhead[0] == EV_CHAT) {
			ev_chat_t *hdr = (ev_chat_t*)&evhead[0];
			uint8_t* chathead = &evhead[sizeof(ev_chat_t)];
			size_t chatlen = strlen( (char*)chathead);
			int whispercid = hdr->dstcid;
			char chatbuf[NET_CHAT_LIMIT];
			if(dlen < chatlen || chatlen >= NET_CHAT_LIMIT) {
				Log(cid, "Chat overflow or too short chat packet.\n");
				DisconnectWithReason(cid, "Bad packet.");
				return;
			}
			CopyAsString(chatbuf, chathead, chatlen);
			//is private msg?
			char tb[NET_CHAT_LIMIT] = "";
			if(0 <= whispercid && whispercid <= MAX_CLIENTS) {
				snprintf(tb, NET_CHAT_LIMIT - 1, "[To cid %d]", whispercid);
			}
			if(chatbuf[0] == '?') {
				Log(cid, "servercommand: %s\n", chatbuf);
				ExecServerCommand(chatbuf, cid);
				copyev = 0; //Do not copy server command chat
			} else {
				Log(cid, "chat%s: %s\n", tb, chatbuf);
			}
		} else if(evhead[0] == EV_RESET) {
			Log(cid, "AddEvent(): round reset request\n");
			if(GetUserOpLevel(cid) < 1) {
				copyev = 0;
				Log(cid, "AddEvent(): Request denied, bad op level.\n");
			}
		} else if(evhead[0] == EV_CHANGE_ATKGAIN) {
			Log(cid, "AddEvent(): change atkgain\n");
			if(GetUserOpLevel(cid) < 1) {
				copyev = 0;
				Log(cid, "AddEvent(): Request denied, bad op level.\n");
			}
		} else if(evhead[0] == EV_PLAYABLE_LOCATION) {
			//Update current client's coordinate information, do not stack the packet.
			ev_playablelocation_t *ev = (ev_playablelocation_t*)evhead;
			C[cid].playable_x = ev->x;
			C[cid].playable_y = ev->y;
			copyev = 0;
		}
		if(copyev == 1) {
			memcpy(&EventBuffer[EBptr + sizeof(event_hdr_t) + pdlen], evhead, evlen);
			pdlen += evlen;
		}
		evcp += evlen;
	}
	if(pdlen != 0) {
		//Append Event header
		event_hdr_t hdr = {
			.cid = (int8_t)cid,
			.evlen = pdlen
		};
		memcpy(&EventBuffer[EBptr], &hdr, sizeof(hdr) );
		EBptr += sizeof(hdr) + pdlen;
	}
}

void GetEvent(int cid) {
	char tb[SIZE_EVENT_BUFFER + (sizeof(player_locations_table_t) * MAX_CLIENTS) + 2];
	tb[0] = NP_EXCHANGE_EVENTS;
	int ltcnt = 0;
	int w_ptr = 2; //send buffer write ptr
	//Copy playable location table before event chunks
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd == -1 || C[i].uid == -1) {
			continue;
		}
		player_locations_table_t table = {
			.cid = i,
			.px = C[i].playable_x,
			.py = C[i].playable_y
		};
		memcpy(&tb[w_ptr], &table, sizeof(player_locations_table_t) );
		ltcnt++;
		w_ptr += sizeof(player_locations_table_t);
	}
	tb[1] = (uint8_t)ltcnt;
	//Copy events to sending buffer, but get rid of the events that has same owner to sender (except chat)
	int elen = EBptr - C[cid].bufcur;
	uint8_t *head = &EventBuffer[C[cid].bufcur];
	int p = 0; //Pointer of event chunk array
	while(p < elen) {
		//Process one event chunk
		event_hdr_t *evchdr = (event_hdr_t*)&head[p];
		int ecid = evchdr->cid; //Event chunk owner
		int eclen = evchdr->evlen; //Event chunk len
		int chunkp = 0; //Read event chunk ptr
		int w_evclen = 0; //Write event chunk ptr
		while(chunkp < eclen) {
			uint8_t *evhead = &head[p + sizeof(event_hdr_t) + chunkp];
			int evlen = eclen - chunkp;
			int copyevent = 1;
			//Get actual event size
			int tl = GetEventPacketSize(evhead, evlen);
			if(tl != -1) {
				evlen = tl;
				//Don't loopback except EV_CHAT and EV_RESET
				 if(evhead[0] != EV_CHAT && evhead[0] != EV_RESET) {
					if(ecid == cid) {
						copyevent = 0;
					}
				
					//Process secret chat (char 0 = destination cid, char1 = 0)
					if(evhead[0] == EV_CHAT) {
						ev_chat_t *hdr = (ev_chat_t*)evhead;
						int dcid = hdr->dstcid;
						if(0 <= dcid && dcid <= MAX_CLIENTS && (cid != dcid && cid != ecid) ) {
							copyevent = 0;
						}
					}
				}
			} else {
				copyevent = 0;
			}
			if(copyevent) {
				//Append event
				memcpy(&tb[w_ptr + sizeof(event_hdr_t) + w_evclen], evhead, evlen);
				w_evclen += evlen;
			}
			chunkp += evlen;
		}
		//Write event chunk header if there's copied data
		if(w_evclen) {
			event_hdr_t w_evchdr = {
				.cid = ecid,
				.evlen = htons(w_evclen)
			};
			memcpy(&tb[w_ptr], &w_evchdr, sizeof(w_evchdr) );
			w_ptr += w_evclen + sizeof(event_hdr_t);
		}
		p += eclen + sizeof(event_hdr_t);
	}
	C[cid].bufcur += elen;
	//Send out data
	send_packet(tb, w_ptr, cid);
	EventBufferGC();
	//Log(cid, "GetEvent()\n");
}


ssize_t GetEventPacketSize(uint8_t *d, int l) {
	ssize_t r = -1;
	if(l < 1) { return -1; } //If data size is not enough to distinguish data type, return -1
	//Get data len according to struct type
	if(d[0] == EV_PLAYABLE_LOCATION) {
		r = sizeof(ev_playablelocation_t);
	} else if (d[0] == EV_PLACE_ITEM) {
		r = sizeof(ev_placeitem_t);
	} else if (d[0] == EV_USE_SKILL) {
		r = sizeof(ev_useskill_t);
	} else if (d[0] == EV_CHAT) {
		ev_chat_t *e = (ev_chat_t*)d;
		size_t chatlen = strnlen(&d[sizeof(ev_chat_t)], NET_CHAT_LIMIT - 1);
		if(chatlen < NET_CHAT_LIMIT) {
			r = sizeof(ev_chat_t) + chatlen + 1;
		}
	} else if (d[0] == EV_RESET) {
		r = sizeof(ev_reset_t);
	} else if (d[0] == EV_HELLO) {
		size_t t = strnlen(&d[1], UNAME_SIZE - 1);
		if(t < UNAME_SIZE) {
			r = t + 2;
		}
	} else if (d[0] == EV_BYE) {
		r = 1;
	} else if(d[0] == EV_CHANGE_PLAYABLE_ID) {
		r = sizeof(ev_changeplayableid_t);
	} else if(d[0] == EV_CHANGE_ATKGAIN) {
		r = sizeof(ev_changeatkgain_t);
	}
	//return -1 if data length is nou enough to store struct
	if(r > l) {
		return -1;
	}
	return r;
}

void EventBufferGC() {
	//Adjust buffer if there's data that read by all clients
	/*
		If there's client 0 and 1,
		+-------------------------------------------------+
	   |                EventBuffer                      |
	   +-------------------------------------------------+
	    <--------->|          |        |
	       ||      |          |      EBptr (Current event head)
		    ||		|			  |
			 ||		|		C[0].bufcur (Client 0 current read pos)
			 ||   C[1].bufcur (Client 1 current read pos)
		 This data can be destroyed since all clients read this data			
	*/
	//Calculate minimum bufcur
	int mincur = EBptr;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd != -1 && mincur > C[i].bufcur) {
			mincur = C[i].bufcur;
		}
	}
	//if(mincur < 0) {
	//	printf("Invalid state: mincur is %d\n", mincur);
	//	return;
	//}
	//Shift actual data
	int slen = EBptr - mincur;
	for(int i = 0; i < slen; i++) {
		EventBuffer[i] = EventBuffer[mincur + i];
	}
	//Shift pointers
	EBptr -= mincur;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd != -1) {
			C[i].bufcur -= mincur;
		}
	}
}

