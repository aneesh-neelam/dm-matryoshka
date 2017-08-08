#ifndef WORKQUEUE_H
#define WORKQUEUE_H

#include <linux/device-mapper.h>

#include "target.h"

void kmatryoshkad_queue_io(struct matryoshka_io *io);
struct bio **kmatryoshkad_init_bios(struct bio *, unsigned int);
inline void kmatryoshkad_init_dev_bio(struct matryoshka_context*, struct bio *, struct matryoshka_device *, struct matryoshka_io *, bio_end_io_t);
void kmatryoshkad_do(struct work_struct *);

#endif /* WORKQUEUE_H */
