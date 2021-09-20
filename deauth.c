#include <asm-generic/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <netdb.h>
#include <linux/sockios.h>
#include <endian.h>
#include <time.h>

struct ieee80211_radiotap_header{
	uint8_t it_version;
	uint8_t it_pad;
	__le16 it_len;
	__le32 it_present;
	uint8_t optional[];
}__attribute__((packed));

struct ieee80211_mac_header{
	__le16 frame_control;
	__le16 duration;
	uint8_t address1[6];
	uint8_t address2[6];
	uint8_t address3[6];
	__le16 seqctl;
	uint8_t body[];
}__attribute__((packed));


struct address_node{
	uint8_t address[6];
	struct address_node *next;
};

uint8_t *get_bssid(int socket, char *ssid){
	char *buffer = calloc(65536, sizeof(char));
	uint8_t *bssid = calloc(6, sizeof(uint8_t));
	int found = 0;
	while(!found){
		if(recvfrom(socket, buffer, 65536, 0, 0, 0) > 0){
			struct ieee80211_radiotap_header *rad_header = (void*)buffer;
			struct ieee80211_mac_header *mac_header = (void*)buffer+rad_header->it_len;
			if(mac_header->frame_control == 0b0000000010000000){
				if(!memcmp(ssid, mac_header->body+14, mac_header->body[13])){
					found = 1;
					memcpy(bssid, mac_header->address3, 6);
				}
			}
		}	
	}
	free(buffer);
	return bssid;
}

struct address_node *get_macs(int socket){
	char *buffer = malloc(65536);
	struct address_node *head = NULL;
	struct address_node *tail = NULL;
	time_t time_end = time(NULL)+10;
	int tods = 0; int fromds = 0;
	while(time(NULL) < time_end){
		bzero(buffer, 65536);
		if(recvfrom(socket, buffer, 65536, 0, 0, 0) > 0){
			struct ieee80211_radiotap_header *rad_header = (void*)buffer;
			struct ieee80211_mac_header *mac_header = (void*)buffer+rad_header->it_len;
			tods = mac_header->frame_control & 0b0000001000000000;
			uint8_t address[6];
			
			if(tods){ memcpy(address, mac_header->address3, 6); }
			else{ memcpy(address, mac_header->address1, 6); }
			int listed = 0;
			struct address_node *i = head;
			while(i != NULL || listed){
				if(!memcmp(i->address, address, 6)){
					listed = 1;
					break;
				}
				i=i->next;
			}
			if(!listed){
				struct address_node *new = malloc(sizeof(struct address_node));
				memcpy(new->address, address, 6);
				if(tail){
					tail->next = new;
					tail = new;
				}else{
					head = tail = new;
				}
			}
		}
	}
	free(buffer);
	return head;
}

int main(int argc, char **argv){
	int sock_raw = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	setsockopt(sock_raw, SOL_SOCKET, SO_BINDTODEVICE, argv[1], strlen(argv[1]));
	struct address_node *head = get_macs(sock_raw);
	for(struct address_node *i = head; i != NULL; i=i->next){
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n", i->address[0], i->address[1], i->address[2], i->address[3], i->address[4], i->address[5]);
	}
	uint8_t *bssid = get_bssid(sock_raw, argv[2]);
	printf("bssid: %02x:%02x:%02x:%02x:%02x:%02x\n", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
	return 0;
}
