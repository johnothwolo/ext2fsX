/* common.h: definitions needed for the undel and the file related parts */
/* of e2undel */

#define FSIZE 8192

int softmagic(unsigned char *buf, int size);
int ascmagic(unsigned char *buf, int size);
int apprentice();
