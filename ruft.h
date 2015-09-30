typedef struct ruft_pkt_info_  
{
    short flags;
    short awnd;
    int   ack_no;
    int   seq_no;
    int   payload_length;
    char  payload[20];
} ruft_pkt_info_t;
