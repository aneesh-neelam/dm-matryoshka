#ifndef WORKQUEUE_H
#define WORKQUEUE_H

#include <linux/device-mapper.h>

#include "target.h"

void wakeup_kmatryoshkad(struct matryoshka_context *);
void kmatryoshkad_queue_bio(struct matryoshka_context *, struct bio *);
struct bio **kmatryoshkad_init_bios(struct bio *, unsigned int);
inline void kmatryoshkad_init_dev_bio(struct bio *, struct matryoshka_device *, struct io *, bio_end_io_t);
void kmatryoshkad_do(struct work_struct *work)

#endif /* WORKQUEUE_H */
