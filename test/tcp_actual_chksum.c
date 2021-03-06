/*

Let's do a simple test with a real packet.
 
0000  00 50 04 35 44 49 00 80  48 AF D8 3E 08 00 45 00   .P.5DI..H..>..E.
0010  00 BC 49 00 00 00 FF 06  F0 E4 C0 A8 00 05 C0 A8   ..I.............
0020  00 01 00 50 04 06 00 AA  00 5F 00 0C 50 62 50 11   ...P....._..PbP.
0030  05 B4 CA CF 00 00 48 54  54 50 2F 31 2E 30 20 32   ......HTTP/1.0.2
0040  30 30 20 4F 4B 0D 0A 43  6F 6E 74 65 6E 74 2D 74   00.OK..Content-t
0050  79 70 65 3A 20 74 65 78  74 2F 68 74 6D 6C 0D 0A   ype:.text/html..
0060  0D 0A 3C 48 54 4D 4C 3E  3C 62 6F 64 79 3E 53 69   ..<HTML><body>Si
0070  6D 6D 53 74 69 63 6B 26  72 65 67 3B 20 57 65 62   mmStick&reg;.Web
0080  20 53 65 72 76 65 72 3C  62 72 3E 75 73 69 6E 67   .Server<br>using
0090  20 61 20 50 49 43 20 31  36 46 38 37 34 3C 62 72   .a.PIC.16F874<br
00A0  3E 62 79 20 44 61 76 69  64 20 43 2E 20 57 69 74   >by.David.C..Wit
00B0  74 20 28 31 32 2F 32 34  2F 30 30 29 3C 2F 62 6F   t.(12/24/00)</bo
00C0  64 79 3E 3C 2F 48 54 4D  4C 3E                     dy></HTML>

TCP header begins at 0x0022. Source IP was 192.168.0.5
and destination IP was 192.168.0.1. Protocol was TCP (0x06).

*/

#include "tcp.h"

int
main(int argc, char** argv)
{   
    
    /* Pseudo-header. We're not going to use it though
    uchar hdr[] = {
        0xC0, 0xA8, 0x00, 0x05, 0xC0, 0xA8, 0x00, 0x01,
        0x00, 0x06, 0x00, 0xA8
    }; */
    
    uchar pkt[] = {
        0x00, 0x50, 0x04, 0x06, 0x00, 0xAA, 0x00, 0x5F,
        0x00, 0x0C, 0x50, 0x62, 0x50, 0x11, 0x05, 0xB4,
        0xCA, 0xCF, 0x00, 0x00, 0x48, 0x54, 0x54, 0x50,
        0x2F, 0x31, 0x2E, 0x30, 0x20, 0x32, 0x30, 0x30,
        0x20, 0x4F, 0x4B, 0x0D, 0x0A, 0x43, 0x6F, 0x6E,
        0x74, 0x65, 0x6E, 0x74, 0x2D, 0x74, 0x79, 0x70,
        0x65, 0x3A, 0x20, 0x74, 0x65, 0x78, 0x74, 0x2F,
        0x68, 0x74, 0x6D, 0x6C, 0x0D, 0x0A, 0x0D, 0x0A,
        0x3C, 0x48, 0x54, 0x4D, 0x4C, 0x3E, 0x3C, 0x62,
        0x6F, 0x64, 0x79, 0x3E, 0x53, 0x69, 0x6D, 0x6D,
        0x53, 0x74, 0x69, 0x63, 0x6B, 0x26, 0x72, 0x65,
        0x67, 0x3B, 0x20, 0x57, 0x65, 0x62, 0x20, 0x53,
        0x65, 0x72, 0x76, 0x65, 0x72, 0x3C, 0x62, 0x72,
        0x3E, 0x75, 0x73, 0x69, 0x6E, 0x67, 0x20, 0x61,
        0x20, 0x50, 0x49, 0x43, 0x20, 0x31, 0x36, 0x46,
        0x38, 0x37, 0x34, 0x3C, 0x62, 0x72, 0x3E, 0x62,
        0x79, 0x20, 0x44, 0x61, 0x76, 0x69, 0x64, 0x20,
        0x43, 0x2E, 0x20, 0x57, 0x69, 0x74, 0x74, 0x20,
        0x28, 0x31, 0x32, 0x2F, 0x32, 0x34, 0x2F, 0x30,
        0x30, 0x29, 0x3C, 0x2F, 0x62, 0x6F, 0x64, 0x79,
        0x3E, 0x3C, 0x2F, 0x48, 0x54, 0x4D, 0x4C, 0x3E
    };

    Data* buf = (Data*)calloc(1, sizeof(Data));
    Header* hdr = (Header*)calloc(1, sizeof(Header));
    
    hdr->src = inet_addr("192.168.0.5");
    hdr->dst = inet_addr("192.168.0.1");
    hdr->prot = htons(IP_PROTO_TCP);
    hdr->tlen = htons(sizeof(pkt));
    
    buf->len = HEADER_OFF + sizeof(pkt);
    buf->content = (uchar*)calloc(buf->len, sizeof(uchar));
    
    memcpy(buf->content, hdr, HEADER_OFF);
    memcpy(buf->content + HEADER_OFF, pkt, sizeof(pkt));
    
    if (inet_checksum(buf->content, buf->len) == 0x00) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
    
    free(buf->content);
    free(buf);
    free(hdr);
    
    return 1;
}
