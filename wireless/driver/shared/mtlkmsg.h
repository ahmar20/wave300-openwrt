/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef _MTLKMSG_H_
#define _MTLKMSG_H_

#if defined (MTCFG_BUS_PCI_PCIE) || defined (MTCFG_BUS_AHB)
#include "mtlk_mmb_msg.h"
#else
#error Unknown HW! Can not include suitable msg.h!
#endif

#endif /* ! _MTLKMSG_H_ */
