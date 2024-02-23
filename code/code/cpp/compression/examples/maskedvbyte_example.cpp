#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <cstring>
#include <iostream>

#include <vector>

#include "varintencode.h"
#include "varintdecode.h"

int main() {
	int N = 5000;
    std::vector<int32_t> datain(N);
    std::vector<uint8_t> compressedbuffer(2 * N * 4);
    std::vector<int32_t> recovdata(N);

	for (int k = 0; k < N; ++k)
		datain[k] = 120 + 2 * k;

	size_t compsize = vbyte_encode(reinterpret_cast<uint32_t *>(datain.data()), N, compressedbuffer.data()); // encoding
	// here the result is stored in compressedbuffer using compsize bytes
	size_t compsize2 = masked_vbyte_decode(compressedbuffer.data(), reinterpret_cast<uint32_t *>(recovdata.data()),
					N); // decoding (fast)
	assert(compsize == compsize2);

	printf("Compressed %d integers down to %d bytes.\n",N,(int) compsize);
	// return 0;

    // Also we can do delta encoding from int32 to int8
    compsize = vbyte_encode_delta(reinterpret_cast<uint32_t *>(datain.data()), N, compressedbuffer.data(), /* prev */ 0); // encoding
	// here the result is stored in compressedbuffer using compsize bytes
	compsize2 = masked_vbyte_decode_delta(compressedbuffer.data(), reinterpret_cast<uint32_t *>(recovdata.data()),
					N, /* prev */ 0); // decoding (fast)
	assert(compsize == compsize2);

	printf("Compressed %d integers down to %d bytes.\n",N,(int) compsize);


    // Can also random access within the delta coding
    uint32_t v = masked_vbyte_select_delta(compressedbuffer.data(), N, /* prev*/ 0, /* slot*/ 10);
    int32_t tmp;
    std::memcpy(&tmp, &v, sizeof(tmp));
    const int32_t v_casted = tmp; // https://stackoverflow.com/a/14623406

    assert(v_casted == datain[10]);

    std::cout << v_casted << " " << datain[10] << "\n";

    return 0;
}

