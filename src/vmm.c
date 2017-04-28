#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "conf.h"
#include "common.h"
#include "vmm.h"
#include "tlb.h"
#include "pt.h"
#include "pm.h"

static unsigned int read_count = 0;
static unsigned int write_count = 0;
static FILE* vmm_log;

int pageFault(unsigned int page,bool write);
unsigned int next = 0;

struct memory{
    unsigned int pageNum;
    bool accessed;
};

struct memory virtualmem[NUM_FRAMES];

void vmm_init (FILE *log)
{
  // Initialise le fichier de journal.
  vmm_log = log;
}



// NE PAS MODIFIER CETTE FONCTION
static void vmm_log_command (FILE *out, const char *command,
                             unsigned int laddress, /* Logical address. */
		                    unsigned int page,
                             unsigned int frame,
                             unsigned int offset,
                             unsigned int paddress, /* Physical address.  */
		             char c) /* Caractère lu ou écrit.  */
{
  if (out)
    fprintf (out, "%s[%c]@%05d: p=%d, o=%d, f=%d pa=%d\n", command, c, laddress,
	     page, offset, frame, paddress);
}

/* Effectue une lecture à l'adresse logique `laddress`.  */
char vmm_read (unsigned int laddress)
{
  char c = '!';
  read_count++;
  /* ¡ TODO: COMPLÉTER ! */
  unsigned int offset = laddress & 255;
  unsigned int page = laddress >> 8;

  unsigned int frame = tlb_lookup(page,0);
  if (frame == -1){
    frame = pt_lookup(page);
    if (frame!=-1){
      tlb_add_entry(page,frame,0);
    }
    else {
        frame = pageFault(page,0);

      }
  }

  unsigned int physaddress = (frame * PAGE_FRAME_SIZE) + offset;
  c = pm_read(physaddress);

  // TODO: Fournir les arguments manquants.
  vmm_log_command (stdout, "READING", laddress, page, frame,offset,physaddress, c);
  return c;
}

/* Effectue une écriture à l'adresse logique `laddress`.  */
void vmm_write (unsigned int laddress, char c)
{
  write_count++;
  /* ¡ TODO: COMPLÉTER ! */

  unsigned int offset = laddress & 255;
  unsigned int page = laddress >> 8;

  unsigned int frame = tlb_lookup(page,0);
  if (frame == -1){
    frame = pt_lookup(page);
    if (frame!=-1){
      tlb_add_entry(page,frame,0);
    }
    else {
      frame = pageFault(page,1);
    }
  }

  unsigned int physaddress = (frame * PAGE_FRAME_SIZE) + offset;
  pm_write(physaddress,c);
  pt_set_readonly(page,0);
  // TODO: Fournir les arguments manquants.
  vmm_log_command (stdout, "WRITING", laddress,page, frame,offset,physaddress, c);
}

int pageFault(unsigned int page,bool write){
  while (virtualmem[next].accessed == true){
    virtualmem[next].accessed = false;
    next = (next + 1) % NUM_FRAMES;
  }
  int frame = next;
  int old = virtualmem[frame].pageNum;
  if (old >= 0 && !pt_readonly_p(old)){
    printf("BACKUP");
    pm_backup_page(frame,old);
      pt_unset_entry(old);
}
  pm_download_page(page,frame);
  pt_set_entry(page,frame);
  pt_set_readonly(page,1);
  tlb_add_entry(page,frame,pt_readonly_p(page));
  virtualmem[frame].pageNum = page;
  next = (next+1)%NUM_FRAMES;
  return frame;
}



// NE PAS MODIFIER CETTE FONCTION
void vmm_clean (void)
{
  fprintf (stdout, "VM reads : %4u\n", read_count);
  fprintf (stdout, "VM writes: %4u\n", write_count);
}
