#ifndef CCISS_H_
#define CCISS_H_

#define CCISS_H_CVSID "$Id$\n"

int cciss_io_interface(int device, int target,
			      struct scsi_cmnd_io * iop, int report);

#endif /* CCISS_H_ */
