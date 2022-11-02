#include <sys/multilist.h>
#include <sys/arc.h>
#include <sys/arc_impl.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/vdev.h>
#include <sys/vdev_impl.h>
#include <sys/metaslab_impl.h>
#include <sys/avl.h>
#include <libnvpair.h>
#include <sys/dmu_objset.h>

static const char cmdname[] = "zdb";

extern unsigned long zfs_arc_meta_min, zfs_arc_meta_limit;
extern boolean_t spa_load_verify_dryrun;

static void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) fprintf(stderr, "%s: ", cmdname);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fprintf(stderr, "\n");

	zfs_dbgmsg_print("zdb");

	exit(1);
}

void inspect_metaslab_group(metaslab_group_t *mg) {
  if (mg == NULL) {
    puts("metaslab group NULL");
    return;
  }
  avl_tree_t *t = &mg->mg_metaslab_tree;
  for (metaslab_t *msp = avl_first(t); msp != NULL; msp = AVL_NEXT(t, msp)) {
    printf("msp %p, id: %ld\n", msp, msp->ms_id);
  }
}

void print_histogram(spa_t *spa, int verbose) {
  objset_t *mos = spa_meta_objset(spa);
  vdev_t *rvd = spa->spa_root_vdev;
  // only show histograms for the normal class
  metaslab_class_t *mc = spa_normal_class(spa);
  uint64_t free_space = 0;

  uint64_t histogram[RANGE_TREE_HISTOGRAM_SIZE];
  for (int i=0; i<RANGE_TREE_HISTOGRAM_SIZE; i++) histogram[i] = 0;

  for (unsigned c = 0; c < rvd->vdev_children; c++) {
    vdev_t *tvd = rvd->vdev_child[c];
    if (0) {
      puts("normal metaslab group");
      inspect_metaslab_group(tvd->vdev_mg);
      puts("log metaslab group");
      inspect_metaslab_group(tvd->vdev_log_mg);
    }
    metaslab_group_t *mg = tvd->vdev_mg;
    if (mg->mg_class != mc) continue;

    uint64_t ms_size = 1ULL << tvd->vdev_ms_shift;

    if (verbose) printf("vdev#%d metaslab array %ld, count %ld, each metaslab is %ldM\n", c, tvd->vdev_ms_array, tvd->vdev_ms_count, ms_size >> 20);
    uint64_t ms_array_size_bytes = sizeof(uint64_t) * tvd->vdev_ms_count;
    uint64_t *metaslab_array = malloc(ms_array_size_bytes);
    VERIFY0(dmu_read(mos, tvd->vdev_ms_array, 0, ms_array_size_bytes, metaslab_array, 0));

    avl_tree_t *t = &mg->mg_metaslab_tree;
    for (metaslab_t *msp = avl_first(t); msp != NULL; msp = AVL_NEXT(t, msp)) {
    //for (int i=0; i<tvd->vdev_ms_count; i++) {
      int metaslab_index = msp->ms_id;
      if (verbose) printf("metaslab %d has spacemap in object %ld\n", metaslab_index, metaslab_array[metaslab_index]);
      dmu_buf_t *db;

      VERIFY0(dmu_bonus_hold(mos, metaslab_array[metaslab_index], FTAG, &db));
      space_map_phys_t *sm  = db->db_data;
      free_space += ms_size - sm->smp_alloc;
      if (verbose) {
        printf("%p %ld\n", sm, sm->smp_object);
        printf("length %ld, alloc %ld, free %ldM\n", sm->smp_length, sm->smp_alloc, (ms_size - sm->smp_alloc)>>20);
      }
      for (int j=0; j<SPACE_MAP_HISTOGRAM_SIZE; j++) {
        if (sm->smp_histogram[j] > 0) {
          if (verbose) printf("%ld %ld\n", tvd->vdev_ashift + j, sm->smp_histogram[j]);
        }
        histogram[tvd->vdev_ashift + j] += sm->smp_histogram[j];
      }
      dmu_buf_rele(db, FTAG);
    }
    free(metaslab_array);
  }

  printf("smp_alloc_free %ld\n", free_space);

  free_space = 0;
  for (int i=0; i<RANGE_TREE_HISTOGRAM_SIZE; i++) {
    free_space += (1ULL << i) * mc->mc_histogram[i];
  }
  printf("metaclass_free %ld\n", free_space);

  free_space = 0;
  for (int i=0; i<RANGE_TREE_HISTOGRAM_SIZE; i++) {
    if (histogram[i] > 0) {
      printf("bucket{power=\"%d\"} %ld\n", i, histogram[i]);
    }
    free_space += (1ULL << i) * histogram[i];
  }
  printf("histogram_free %ld\n", free_space);
}

static void
zdb_print_blkptr(const blkptr_t *bp)
{
	char blkbuf[BP_SPRINTF_LEN];


	snprintf_blkptr(blkbuf, sizeof (blkbuf), bp);
	(void) printf("%s\n", blkbuf);
}

int main(int argc, char **argv) {
  nvlist_t *policy = NULL;
  spa_t *spa = NULL;
  assert(argc == 2);
  char *target = argv[1];
  int error = 0;

  /*
   * ZDB does not typically re-read blocks; therefore limit the ARC
   * to 256 MB, which can be used entirely for metadata.
   */
  zfs_arc_min = zfs_arc_meta_min = 2ULL << SPA_MAXBLOCKSHIFT;
  zfs_arc_max = zfs_arc_meta_limit = 256 * 1024 * 1024;

  spa_load_verify_dryrun = B_TRUE;

  kernel_init(SPA_MODE_READ);

  if (nvlist_alloc(&policy, NV_UNIQUE_NAME_TYPE, 0) != 0)
    fatal("internal error: %s", strerror(ENOMEM));

  error = spa_open(target, &spa, FTAG);
  if (error != 0) {
    fatal("Tried to open pool \"%s\" but "
        "spa_open() failed with error %d\n",
        target, error);
  }

  print_histogram(spa, B_FALSE);

  for (int i=0; i<10; i++) {
    vdev_t *rvd = spa->spa_root_vdev;
    struct uberblock ub;
    nvlist_t *config = NULL;
    vdev_uberblock_load(rvd, &ub, &config);
    printf("magic: 0x%lx, txt: %ld\n", ub.ub_magic, ub.ub_txg);
    if (ub.ub_guid_sum != spa->spa_uberblock.ub_guid_sum) {
      printf("guid sum differs, reload needed\n");
      exit(1);
    }
    zdb_print_blkptr(&ub.ub_rootbp);
    if (config) {
      dump_nvlist(config, 0);
      nvlist_free(config);
      config = NULL;
    }
    arc_flags_t aflags = ARC_FLAG_WAIT; // block on arc miss
    zbookmark_phys_t zb;
    arc_buf_t *arcbuf;
    int err = arc_read(NULL, spa, &ub.ub_rootbp, arc_getbuf_func, &arcbuf, ZIO_PRIORITY_SYNC_READ, ZIO_FLAG_CANFAIL, &aflags, &zb);
    if (err != 0) {
      puts("arc read error");
      exit(1);
    }
    printf("arcbuf: %p\n", arcbuf);
    objset_t os;
    memset(&os, 0, sizeof(objset_t));
    sleep(30);
  }

  //show_pool_stats(spa);
  return 0;
}
