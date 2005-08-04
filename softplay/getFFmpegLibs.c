#include <stdio.h>
#include <avcodec.h>

int main() {
        if ( LIBAVCODEC_BUILD > 4795 )
	  printf("-lavutil\n");
        };
