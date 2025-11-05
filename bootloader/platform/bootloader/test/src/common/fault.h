#ifndef FAULT_H
#define FAULT_H

typedef void (*handler_fn_t)(void);

void faultInit(void);
void faultSetHandler(handler_fn_t h);

#endif
