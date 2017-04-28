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
static FILE *vmm_log;
int count = 0;

int pageFault(unsigned int page, int frame, bool write);

unsigned int next = 0;

struct memory {
    unsigned int pageNum;
    bool accessed;
};

struct memory virtualmem[NUM_FRAMES];

void vmm_init(FILE *log) {
    // Initialise le fichier de journal.
    vmm_log = log;
}


// NE PAS MODIFIER CETTE FONCTION
static void vmm_log_command(FILE *out, const char *command,
                            unsigned int laddress, /* Logical address. */
                            unsigned int page,
                            unsigned int frame,
                            unsigned int offset,
                            unsigned int paddress, /* Physical address.  */
                            char c) /* Caractère lu ou écrit.  */
{
    if (out)
        fprintf(out, "%s[%c]@%05d: p=%d, o=%d, f=%d pa=%d\n", command, c, laddress,
                page, offset, frame, paddress);
}

/* Effectue une lecture à l'adresse logique `laddress`.  */
char vmm_read(unsigned int laddress) {
    char c = '!';
    read_count++;

    /* ¡ TODO: COMPLÉTER ! */
    //Calcule de l'offset et page number
    unsigned int offset = laddress & 255;
    unsigned int page = laddress >> 8;

    //Recherche du frame number dans TLB
    unsigned int frame = tlb_lookup(page, 0);

    //Si le frame n'est pas trouvé dans le TLB
    if (frame == -1) {
        //Recherche dans la table des pages
        if ((frame = pt_lookup(page)) != -1) {
            //Si trouvé dans la table des pages ajouter à TLB
            tlb_add_entry(page, frame, 0);
        } else {
            //Sinon Page Fault
            frame = pageFault(page, frame, 0);
        }
    }
    //Calcule l'adresse physique
    unsigned int physaddress = (frame * PAGE_FRAME_SIZE) + offset;
    //Lire le caractère de la mémoire physique
    c = pm_read(physaddress);

    // TODO: Fournir les arguments manquants.
    vmm_log_command(stdout, "READING", laddress, page, frame, offset, physaddress, c);
    return c;
}

/* Effectue une écriture à l'adresse logique `laddress`.  */
void vmm_write(unsigned int laddress, char c) {
    write_count++;
    /* ¡ TODO: COMPLÉTER ! */

    //Calcule de l'offset et page number
    unsigned int offset = laddress & 255;
    unsigned int page = laddress >> 8;

    //Recherche du frame number dans TLB
    unsigned int frame = tlb_lookup(page, 1);

    //Si le frame n'est pas trouvé dans le TLB
    if (frame == -1) {
        //Si trouvé dans la table des pages ajouter à TLB
        if ((frame = pt_lookup(page)) != -1) {
            //Si trouvé dans la table des pages ajouter à TLB
            tlb_add_entry(page, frame, 1);
        } else {
            //Sinon Page Fault
            frame = pageFault(page, frame, 1);
        }
    }
    //Calcule l'adresse physique
    unsigned int physaddress = (frame * PAGE_FRAME_SIZE) + offset;
    //Écrire le caractère fourni dans la mémoire physique
    pm_write(physaddress, c);
    //Mettre readonly à false (page modifié)
    pt_set_readonly(page, 0);

    // TODO: Fournir les arguments manquants.
    vmm_log_command(stdout, "WRITING", laddress, page, frame, offset, physaddress, c);
}

//Page Fault
int pageFault(unsigned int page, int frame, bool write) {
    //Algorithme de remmplacement Second chance(Clock)
    while (virtualmem[next].accessed == true) {
        virtualmem[next].accessed = false;
        next = (next + 1) % NUM_FRAMES;
    }
    //Trouver un frame avec accessed(referenced) = 0
    frame = next;
    int old = virtualmem[frame].pageNum;
    //Si la page n'est pas vide déjà modifié(dirty bit)
    if (old >= 0 && !pt_readonly_p(old)) {
        //Copier le contenu de la page dans le backstore
        pm_backup_frame(frame, old);
        //Mettre la page à invalid
        pt_unset_entry(old);
    }
    //Télécharger la page du backing store
    pm_download_page(page, frame);
    //Ajouter la page dans la table des pages
    pt_set_entry(page, frame);
    //Mettre la page à readonly
    pt_set_readonly(page, 1);
    //Ajouter la page à TLB
    tlb_add_entry(page, frame, pt_readonly_p(page));
    virtualmem[frame].pageNum = page;
    virtualmem[frame].accessed = true;
    next = (next + 1) % NUM_FRAMES;

    return frame;
}


// NE PAS MODIFIER CETTE FONCTION
void vmm_clean(void) {
    fprintf(stdout, "VM reads : %4u\n", read_count);
    fprintf(stdout, "VM writes: %4u\n", write_count);
}
