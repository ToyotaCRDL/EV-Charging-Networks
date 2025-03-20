#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "dSFMT.h"

typedef struct {
  int i;
  int j;
  double dist;
} link_t;

typedef struct {
  double lat;		// 緯度 [0,1]
  double lon;		// 経度 [0,1]
} node_t;

dsfmt_t dsfmt;

int core(node_t node_a[], node_t node_b[], link_t link_a[], link_t link_b[], int INI_NODE_NUM_A, int INI_NODE_NUM_B, double LOWER_DIST_A, double UPPER_DIST_A, double LOWER_DIST_B, double UPPER_DIST_B, int AREA_SIZE, char *OUT_DIR);
void add_xy_on_dense_layer(node_t node[], int INI_NODE_NUM, int AREA_SIZE);
void add_xy_on_sparse_layer(node_t node_a[], node_t node_b[], int INI_NODE_NUM_A, int INI_NODE_NUM_B);
int generate_link(node_t node[], link_t link[], int INI_NODE_NUM, double LOWER, double UPPER);
void output_result(node_t node_a[], node_t node_b[], link_t link_a[], link_t link_b[], int INI_NODE_NUM_A, int INI_NODE_NUM_B, double LOWER_DIST_A, double UPPER_DIST_A, double LOWER_DIST_B, double UPPER_DIST_B, int AREA_SIZE, int link_num_a, int link_num_b, char *OUT_DIR);

int main(int argc, char *argv[]) {
  node_t *node_a;				// ノードリスト (Layer A)
  node_t *node_b;				// ノードリスト (Layer B)
  link_t *link_a;				// リンクリスト (Layer A)
  link_t *link_b;				// リンクリスト (Layer B)
  
  //                    argv[1]			// Output Directory
  int INI_NODE_NUM_A  = atoi(argv[2]);		// Initial number of nodes on Layer A
  int INI_NODE_NUM_B  = atoi(argv[3]);		// Initial number of nodes on Layer B
  double LOWER_DIST_A = atof(argv[4]);		// Lower bound of distance between ij on Layer A
  double UPPER_DIST_A = atof(argv[5]);		// Upper bound of distance between ij on Layer A
  double LOWER_DIST_B = atof(argv[6]);		// Lower bound of distance between ij on Layer B
  double UPPER_DIST_B = atof(argv[7]);		// Upper bound of distance between ij on Layer B
  double AREA_SIZE    = atoi(argv[8]);		// Area size
  int SEED            = atoi(argv[9]);          // 乱数のシード
  
  /* 配列の確保 etc. */
  node_a = (node_t *)calloc(10000000, sizeof(node_t));
  node_b = (node_t *)calloc(10000000, sizeof(node_t));
  link_a = (link_t *)calloc(100000000, sizeof(link_t));
  link_b = (link_t *)calloc(100000000, sizeof(link_t));
  if (node_a == NULL || node_b == NULL || link_a == NULL || link_b == NULL) {
    printf("###ERROR### メモリを確保できませんでした\n");
    exit(1);
  }
  
  /* 乱数の生成 */
  dsfmt_init_gen_rand(&dsfmt, SEED);
  
  /* Main */
  core(node_a, node_b, link_a, link_b, INI_NODE_NUM_A, INI_NODE_NUM_B, LOWER_DIST_A, UPPER_DIST_A, LOWER_DIST_B, UPPER_DIST_B, AREA_SIZE, argv[1]);
  
  /* 後処理 */
  free(node_a);
  free(node_b);
  free(link_a);
  free(link_b);
  
  return 0;
}

int core(node_t node_a[], node_t node_b[], link_t link_a[], link_t link_b[], int INI_NODE_NUM_A, int INI_NODE_NUM_B, double LOWER_DIST_A, double UPPER_DIST_A, double LOWER_DIST_B, double UPPER_DIST_B, int AREA_SIZE, char *OUT_DIR) {
  int link_num_a, link_num_b;
  char time_start[100], time_end[100];
  time_t time_now;
  struct tm *tm_now;
  
  time(&time_now);
  tm_now = localtime(&time_now);
  strftime(time_start, 10000, "%y%m%d_%H%M%S", tm_now);
  
  // Add Geometric Location
  add_xy_on_dense_layer(node_a, INI_NODE_NUM_A, AREA_SIZE);
  add_xy_on_sparse_layer(node_a, node_b, INI_NODE_NUM_A, INI_NODE_NUM_B);
  
  // Generate links
  link_num_a = generate_link(node_a, link_a, INI_NODE_NUM_A, LOWER_DIST_A, UPPER_DIST_A);
  link_num_b = generate_link(node_b, link_b, INI_NODE_NUM_B, LOWER_DIST_B, UPPER_DIST_B);
  
  // Output
  output_result(node_a, node_b, link_a, link_b, INI_NODE_NUM_A, INI_NODE_NUM_B, LOWER_DIST_A, UPPER_DIST_A, LOWER_DIST_B, UPPER_DIST_B, AREA_SIZE, link_num_a, link_num_b, OUT_DIR);
  
  time(&time_now);
  tm_now = localtime(&time_now);
  strftime(time_end, 10000, "%y%m%d_%H%M%S", tm_now);
  
  return 0;
}

/*-------------------------------------
  Add Geometric Location on Dense Layer
  -------------------------------------*/
void add_xy_on_dense_layer(node_t node[], int INI_NODE_NUM, int AREA_SIZE) {
  int i;
  
  for (i = 0; i < INI_NODE_NUM; i++) {
    node[i].lat = AREA_SIZE * dsfmt_genrand_close_open(&dsfmt);
    node[i].lon = AREA_SIZE * dsfmt_genrand_close_open(&dsfmt);
  }
}

/*--------------------------------------
  Add Geometric Location on Sparse Layer
  --------------------------------------*/
void add_xy_on_sparse_layer(node_t node_a[], node_t node_b[], int INI_NODE_NUM_A, int INI_NODE_NUM_B) {
  int i, selected_node_position;
  
  for (i = 0; i < INI_NODE_NUM_B; i++) {
    selected_node_position = (int)(INI_NODE_NUM_A * dsfmt_genrand_close_open(&dsfmt));
    node_b[i].lat = node_a[selected_node_position].lat;
    node_b[i].lon = node_a[selected_node_position].lon;
  }
}

/*--------------
  Generate Links 
  --------------*/
int generate_link(node_t node[], link_t link[], int INI_NODE_NUM, double LOWER, double UPPER) {
  int i, j, k;
  double dist, x_diff, y_diff;
  
  k = 0;
  // 全ての二点間の距離を計算
  for (i = 0; i < INI_NODE_NUM; i++) {
    for (j = 0; j < INI_NODE_NUM; j++) {
      if (i < j) {
	
	// ユーグリット距離
	x_diff = node[i].lat - node[j].lat;
	y_diff = node[i].lon - node[j].lon;
	dist = sqrt( pow(x_diff, 2.0) + pow(y_diff, 2.0) );
	
	// 条件に沿う距離だったらリンクを生成
	if ((LOWER <= dist) && (dist <= UPPER)) {
	  link[k].i = i;
	  link[k].j = j;
	  link[k].dist = dist;
	  k++;
	}
      }
    }
  }
  
  return k;
}

void output_result(node_t node_a[], node_t node_b[], link_t link_a[], link_t link_b[], int INI_NODE_NUM_A, int INI_NODE_NUM_B, double LOWER_DIST_A, double UPPER_DIST_A, double LOWER_DIST_B, double UPPER_DIST_B, int AREA_SIZE, int link_num_a, int link_num_b, char *OUT_DIR) {
  int i;
  char filename[10000], filename2[10000];
  FILE *fp, *fp2;
  
  // 出力ファイル ... リンク情報
  sprintf(filename, "%s/dense_network_N_%d_range_%1.2f_%1.2f_area_%d.csv", OUT_DIR, INI_NODE_NUM_A, LOWER_DIST_A, UPPER_DIST_A, AREA_SIZE);
  if ((fp = fopen(filename, "w")) == NULL) {
    printf("###ERROR### 出力ファイル（リンク1）を開けませんでした\n");
    exit(1);
  }
  for (i = 0; i < link_num_a; i++) {
    fprintf(fp, "%d,%d,%f,%f,%f,%f,%f\n", link_a[i].i, link_a[i].j, node_a[link_a[i].i].lat, node_a[link_a[i].i].lon, node_a[link_a[i].j].lat, node_a[link_a[i].j].lon, link_a[i].dist);
  }
  fclose(fp);
  
  // 出力ファイル ... リンク情報
  sprintf(filename2, "%s/sparse_network_N_%d_range_%1.2f_%1.2f_area_%d.csv", OUT_DIR, INI_NODE_NUM_B, LOWER_DIST_B, UPPER_DIST_B, AREA_SIZE);
  if ((fp2 = fopen(filename2, "w")) == NULL) {
    printf("###ERROR### 出力ファイル（リンク2）を開けませんでした\n");
    exit(1);
  }
  for (i = 0; i < link_num_b; i++) {
    fprintf(fp2, "%d,%d,%f,%f,%f,%f,%f\n", link_b[i].i, link_b[i].j, node_b[link_b[i].i].lat, node_b[link_b[i].i].lon, node_b[link_b[i].j].lat, node_b[link_b[i].j].lon, link_b[i].dist);
  }
  fclose(fp2);
}
