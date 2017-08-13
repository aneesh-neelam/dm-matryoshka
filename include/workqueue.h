#ifndef WORKQUEUE_H
#define WORKQUEUE_H

#include <linux/device-mapper.h>

#include "target.h"

void kmatryoshkad_queue_io(struct matryoshka_io *io);
void kmatryoshkad_do(struct work_struct *);

#endif /* WORKQUEUE_H */
