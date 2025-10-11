#include <stdio.h>
#include "geometry.h"
#include "platform_layer.h"

result init(init_in_params* in, init_out_params* out) {
    puts("Hello from platform layer init!");
    return RESULT_SUCCESS;
}

result update(update_params* in) {
    puts("Hello from platform layer update!");
    return RESULT_SUCCESS;
}

void shutdown(shutdown_params* in) {
    puts("Hello from platform layer shutdown!");
}

/*
int main(void) {
    return 0;
}
    */