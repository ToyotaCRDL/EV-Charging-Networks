#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include "dSFMT.h"

typedef struct {
  int i;
  int j;
  int layer;
  double dist;
  double cost;
} link_t;

typedef struct {
  int deg;
  int queue; 		// number of agent stacks
  int layer; 		// layer ... 1 or 2 or 3(1->2 or 2->1)
  int service_num;	// エージェント排出数（毎ステップ初期化する）
  int all_charger_num;	// 全充電設備数
  int used_charger_num;	// 使用中の充電設備数
  int history;		// 使用頻度
} node_t;

typedef struct {
  int t;
  int failure; 		// 充電できなかったエージェント数
  int new; 		// 新たに充電できたエージェント数
  int charging;		// 充電中のエージェント数
  int skip;		// 充電が必要なかったエージェント数
} infra_t;

typedef struct {
  int t_in;		// 参入時刻
  int t_queue_in;	// 現在スタックされているノードへの参入時刻
  int t_now;		// 
  int t_out;		// 退出時刻
  int origin;		// 出発地
  int dest;		// 目的地
  int current; 		// 現在地
  int hop;		// number of moving
  int eval;             // 移動中(0), 目的地にたどり着けた(1), 途中で消滅した(2)
  int soc;              // 充電状態(SoC). 0~10の11段階. 1ホップ移動するごとに1段階減る.
  double soc_limit;     // 充電を開始するSoC
  int charging_flag;    // 充電中(1), 移動中(0). SoCが10になるまで充電フラグは0にならない.
} agent_t;

typedef struct {
  int success;		// 
  int next;		// Next new agent's ID
  int node; 		// number of nodes
  int link; 		// number of links
  int ini_agent_num;    // initial number of agents
  int max_soc;		// SoC
  double lambda;	// lambda for exp dist.
  int max_resource;	// B
  int max_service;	// O
  int max_step_num;	// number of steps for a simulation
  double gamma;		// velocity on Layer2 (velocity = 1 on Layer1)
} info_t;

dsfmt_t dsfmt;
//const int INTERVAL_STEP = 1000;		// 結果出力の間隔

double rand_normal(double mu, double sigma);
double rand_exp(double lambda);
int preferential_selection(int node_num, node_t node[], int flag);
int dijkstra(int node_num, int link_num, node_t node[], link_t link[], int max_resource, double gamma, agent_t agent[], int src);
int minDistance(int node_num, double dist[], bool sptSet[]);
int comp(const void *a, const void *b);

int core(info_t info, node_t node[], link_t link[], agent_t agent[], infra_t infra[], char *OUT_DIR);
int agent_creation_process(int t, int agent_creation_num, int next_agent_id, agent_t agent[], int node_num, node_t node[], int max_resource, int max_soc);
info_t agent_forwarding_process(int t, info_t info, int next_agent_id, int node_num, node_t node[], int link_num, link_t link[], agent_t agent[], infra_t infra[], int max_resource, int max_soc, int max_service, double gamma);
void node_updating_process(int node_num, node_t node[], int max_resource);
void output_result(agent_t agent[], int ini_agent_num, int next_agent_id, node_t node[], int node_num, infra_t infra[], int t, int max_resource, int max_soc, double lambda, int max_service, double gamma, char *OUT_DIR);

// Random value followed Normal distribution
double rand_normal(double mu, double sigma) {
  double uniform = ((double)rand()+1.0)/((double)RAND_MAX+2.0);
  double z = sqrt( -2.0*log(uniform) ) * sin( 2.0*M_PI*uniform );
  double result = mu + sigma*z;
  if (result < 0) {
    return 0;
  } else {
    return result;
  }
}

// Random value followed Exponential distribution
double rand_exp(double lambda) {
  double uniform = ((double)rand()+1.0)/((double)RAND_MAX+2.0);
  double result = round(-log(uniform)/lambda);
  if (result < 0) {
    return 0;
  } else {
    return result;
  }
}

// Select a node with Preferential Attachment Effect
int preferential_selection(int node_num, node_t node[], int flag) {
  int i;
  double pa_prob, pa_sum;
  
  if (flag == 1) {
    
    // ランダムに選択
    i = floor(node_num * dsfmt_genrand_close_open(&dsfmt));
    
  } else {
    
    // 次数に沿ってランダムに選択
    pa_sum = 0.0;
    for (i = 0; i < node_num; i++) {
      pa_sum += pow(node[i].deg, 1.0) + 1.0;
    }
    pa_prob = pa_sum * dsfmt_genrand_close_open(&dsfmt);
    
    i = -1; pa_sum = 0.0;
    do {
      i++;
      pa_sum += pow(node[i].deg, 1.0) + 1.0;
    } while (pa_prob > pa_sum);
  }
  
  return i;
}

// Search a node with minimum cost
int minDistance(int node_num, double dist[], bool sptSet[]) {
  int i, min_index;
  double min = DBL_MAX;
  
  for (i = 0; i < node_num; i++) {
    if (sptSet[i] == false && dist[i] <= min) {
      min = dist[i];
      min_index = i;
    }
  }
  
  return min_index;
}

// Get shortest path for a src node
int dijkstra(int node_num, int link_num, node_t node[], link_t link[], int max_resource, double gamma, agent_t agent[], int src) {
  int i, cnt, u, v, start, end;
  double w;
  double dist[node_num];
  int parent[node_num];
  bool sptSet[node_num];
  
  // Initialize
  double graph[node_num][node_num];
  for (i = 0; i < link_num; i++) {
    if (max_resource <= node[link[i].j].queue) {
      graph[link[i].i][link[i].j] = DBL_MAX;
    } else {
      
      // そろそろ充電したい場合
      if (((double)agent[src].soc - agent[src].soc_limit) <= 1.0) {
	if (node[link[i].j].used_charger_num == node[link[i].j].all_charger_num) {
	  w = 10000;
	} else {
	  w = (double)node[link[i].j].used_charger_num / ((double)node[link[i].j].all_charger_num - (double)node[link[i].j].used_charger_num);
	}

      // まだ充電が不要な場合
      } else {
	w = (double)node[link[i].j].queue / ((double)max_resource - (double)node[link[i].j].queue);
      }
      
      if (link[i].layer == 2) {
	graph[link[i].i][link[i].j] = gamma + w;
      } else {
	graph[link[i].i][link[i].j] = 1.0 + w;
      }
    }
  }
  
  for (i = 0; i < node_num; i++) {
    dist[i] = DBL_MAX;
    parent[i] = INT_MAX;
    sptSet[i] = false;
  }
  
  // 始点(start)の設定
  start = agent[src].current;
  dist[start] = 0.0;
  
  // 最短経路の探索
  for (cnt = 0; cnt < node_num-1; cnt++) {
    u = minDistance(node_num, dist, sptSet);
    sptSet[u] = true;
    
    for (v = 0; v < node_num; v++) {
      if (sptSet[v] == false && graph[u][v] && dist[u] != DBL_MAX && dist[u] + graph[u][v] < dist[v]) {
	parent[v] = u;
	dist[v] = (double)dist[u] + (double)graph[u][v];
      }
    }
  }
  
  // 終点の取得
  end = agent[src].dest;
  
  // 終点に向けて次に進むノードの取得
  int bfr, nxt = end;
  do {
    bfr = nxt;
    nxt = parent[nxt];
    
    // どのルートでも目的地までの最短経路が計算できず, 移動できなくなった場合
    if (nxt == INT_MAX) {
      return start;
    }
  } while (nxt != start);
  
  return bfr;
}

// 参入時刻で構造体をソート (Fast In Fast Out)
int comp(const void *a, const void *b) {
  if (((agent_t *)a)->t_queue_in > ((agent_t *)b)->t_queue_in) {
    return 1;  // 降順にするのであれば-1を返す
  } else {
    return -1; // 上とは逆の値を返す
  }
}

int main(int argc, char *argv[]) {
  node_t *node;					// ノードリスト
  link_t *link;					// リンクリスト
  agent_t *agent;				// リスト
  infra_t *infra;				// リスト
  info_t info;					// ノード数 etc.
  FILE *fp1,*fp2;				// 入力ファイル
  
  //                      argv[1]		// Output Directory
  //                      argv[2]               // Input File (Nodes)
  //                      argv[3]               // Input File (Network)
  double MAX_RESOURCE = atof(argv[4]);		// B
  double GAMMA        = atof(argv[5]);		// velocity on Layer2 (velocity = 1 on Layer1)
  int MAX_SOC         = atoi(argv[6]);		// Maximum SoC
  double LAMBDA       = atof(argv[7]);		// lambda for exponential dist.
  int SERVICE         = atoi(argv[8]);		// Maximum service rate
  int STEP_NUM        = atoi(argv[9]);		// Number of steps for a simulation
  int INI_AGENT_NUM   = atoi(argv[10]);		// 初期状態におけるエージェントの総数
  int SEED            = atoi(argv[11]);		// 乱数のシード
  
  /* 配列の確保 etc. */
  node = (node_t *)calloc(10000000, sizeof(node_t));
  link = (link_t *)calloc(100000000, sizeof(link_t));
  agent = (agent_t *)calloc(100000000, sizeof(agent_t));
  infra = (infra_t *)calloc(100000000, sizeof(infra_t));
  if (node == NULL || link == NULL || agent == NULL || infra == NULL) {
    printf("###ERROR### メモリを確保できませんでした\n");
    exit(1);
  }
  
  /* 乱数の生成 */
  dsfmt_init_gen_rand(&dsfmt, SEED);
  
  /* 入力ファイルのチェック */
  if ((fp1 = fopen(argv[2], "r")) == NULL) {
    printf("###ERROR### 入力ファイル（ノード）を開けませんでした\n");
    exit(1);
  }
  if ((fp2 = fopen(argv[3], "r")) == NULL) {
    printf("###ERROR### 入力ファイル（ネットワーク）を開けませんでした\n");
    exit(1);
  }
  
  /* ファイルの読み込み ... id(%d),outdeg(%d),indeg(%d),deg(%d) */
  int id, outdeg, indeg, deg;
  int cnt = 0;
  while( fscanf(fp1, "%d,%d,%d,%d\n", &id,&outdeg,&indeg,&deg ) != EOF ){
    node[id].deg = deg;
    node[id].queue = 0;
    node[id].layer = 1;
    node[id].service_num = 0;
    node[id].all_charger_num = rand_exp(LAMBDA);
//    node[id].all_charger_num = 10000;
    node[id].used_charger_num = 0;
    node[id].history = 0;
    if (cnt >= 300) {
      printf("###ERROR### ノードファイル(1)エラー\n");
      exit(1);
    }
    cnt++;
  }
  int node_num = id + 1;
  fclose(fp1);
  printf("###END### ノードファイル(1)を読み込みました\n");
  
  /* ファイルの読み込み ... from(%d),to(%d),layer(%d),dist(%lf) */
  int from, to, layer;
  double dist;
  
  int i = 0;
  while( fscanf(fp2, "%d,%d,%d,%lf\n", &from,&to,&layer,&dist ) != EOF ){
    link[i].i = from;
    link[i].j = to;
    link[i].layer = layer;
    link[i].dist = dist;
    link[i].cost = 0.0;
    i++;
  }
  int link_num = i;
  fclose(fp2);
  printf("###END### ネットワークファイル(2)を読み込みました\n");
  
  /* 初期値の設定 */
  info.success = 0;
  info.next = 0;
  info.ini_agent_num = INI_AGENT_NUM;
  info.node = node_num;
  info.link = link_num;
  info.max_resource = MAX_RESOURCE;
  info.lambda = LAMBDA;
  info.max_soc = MAX_SOC;
  info.max_service = SERVICE;
  info.max_step_num = STEP_NUM;
  info.gamma = GAMMA;
  
  /* Main */
  core(info, node, link, agent, infra, argv[1]);
  
  /* 後処理 */
  free(node);
  free(link);
  free(agent);
  free(infra);
  
  return 0;
}

int core(info_t info, node_t node[], link_t link[], agent_t agent[], infra_t infra[], char *OUT_DIR) {
  int t, bfr_scs;
  int ini_agent_num = info.ini_agent_num;
  int next_agent_id = info.next;
  int node_num      = info.node;
  int link_num      = info.link;
  int max_resource  = info.max_resource;
  int max_soc       = info.max_soc;
  double lambda     = info.lambda;
  int max_service   = info.max_service;
  int max_step_num  = info.max_step_num;
  double gamma      = info.gamma;
  
  char time_start[100], time_end[100];
  time_t time_now;
  struct tm *tm_now;
  
  time(&time_now);
  tm_now = localtime(&time_now);
  strftime(time_start, 10000, "%y%m%d_%H%M%S", tm_now);
  
  // Main
  int agent_creation_num = ini_agent_num;
  for (t = 0; t < max_step_num; t++) {
    printf("%d step ... next agent id %d (success %d, next_creation_num %d (ini_agent_num %d, max_soc %d)) by l=%1.9f, G=%1.3f, B=%d\n", t, next_agent_id, info.success, agent_creation_num, ini_agent_num, max_soc, lambda, gamma, max_resource);
    
    /*------------------------*/
    /* Agent Creation Process */
    /*------------------------*/
    next_agent_id = agent_creation_process(t, agent_creation_num, next_agent_id, agent, node_num, node, max_resource, max_soc);
    
    /*--------------------------*/
    /* Agent Forwarding Process */
    /*--------------------------*/
    bfr_scs = info.success;
    info = agent_forwarding_process(t, info, next_agent_id, node_num, node, link_num, link, agent, infra, max_resource, max_soc, max_service, gamma);
    agent_creation_num = info.success - bfr_scs;
    
    // 参入時刻で構造体をソート for Fast In Fast Out ... qsort(配列, 配列数, 型のサイズ, ソートする関数)
    qsort(agent, next_agent_id, sizeof(agent_t), comp);
    
    /*-----------------------*/
    /* Node Updating Process */
    /*-----------------------*/
    node_updating_process(node_num, node, max_resource);
  }
  
  // Output
  output_result(agent, ini_agent_num, next_agent_id, node, node_num, infra, t, max_resource, max_soc, lambda, max_service, gamma, OUT_DIR);
  
  time(&time_now);
  tm_now = localtime(&time_now);
  strftime(time_end, 10000, "%y%m%d_%H%M%S", tm_now);
  
  return 0;
}

/*----------------------
  Agent Creation Process
  ----------------------*/
int agent_creation_process(int t, int agent_creation_num, int next_agent_id, agent_t agent[], int node_num, node_t node[], int max_resource, int max_soc) {
  int cnt, i, dest_pos;
  
  for (cnt = 0; cnt < agent_creation_num; cnt++) {

    // エージェントを配置するノードはランダムに選択
    do {
      i = floor(node_num * dsfmt_genrand_close_open(&dsfmt));
    } while (max_resource <= node[i].queue);
    
    // Initialization
    agent[next_agent_id].t_in = t;
    agent[next_agent_id].t_queue_in = t;
    agent[next_agent_id].t_now = t;
    agent[next_agent_id].t_out = -1;
    agent[next_agent_id].eval = 0;
    agent[next_agent_id].soc = max_soc;
    agent[next_agent_id].soc_limit = rand_normal(3.0, 0.01);
    agent[next_agent_id].charging_flag = 0;
    node[i].queue += 1;
    
    // 出発地(Origin)
    agent[next_agent_id].origin  = i;
    agent[next_agent_id].current = i;
    
    // 目的地(Destination)をランダムに選択 (1:ランダム, 2:次数に沿って選択)
    do {
      dest_pos = preferential_selection(node_num, node, 1);
    } while (dest_pos == i); // 出発地と目的地は別にする
    agent[next_agent_id].dest = dest_pos;

    next_agent_id += 1;
  }
  
  return next_agent_id;
}

/*------------------------
  Agent Forwarding Process
  ------------------------*/
info_t agent_forwarding_process(int t, info_t info, int next_agent_id, int node_num, node_t node[], int link_num, link_t link[], agent_t agent[], infra_t infra[], int max_resource, int max_soc, int max_service, double gamma) {
  int src, node_position, next_position;
  int success = info.success;
  
  for (src = 0; src < next_agent_id; src++) {
    if ((agent[src].t_now == t)&&(agent[src].eval == 0)) {
      node_position = agent[src].current;
      node[node_position].history += 1;
      
      // SoCが0の場合は充電する
      if (agent[src].soc <= agent[src].soc_limit) {
	// 充電設備がいっぱいで充電できない場合
	if (node[node_position].used_charger_num == node[node_position].all_charger_num) {
	  infra[t].failure += 1;
	// 充電設備に空きがあって充電できる場合
	} else {
	  agent[src].charging_flag = 1;
	  node[node_position].used_charger_num += 1;
	  infra[t].new += 1;
	}
      }
      
      // 充電中の場合は待機し充電する
      if (agent[src].charging_flag == 1) {
	agent[src].t_now += 1;
	agent[src].soc += 1;
	if (agent[src].soc == max_soc) {
	  agent[src].charging_flag = 0;
	  node[node_position].used_charger_num -= 1;
	}
	infra[t].charging += 1;
	
      // 移動中の場合は次の移動先を探す
      } else {
	infra[t].skip += 1;
	
	// ノードのエージェント排出限界数を超えていたらそこで終了
	if (node[node_position].service_num >= max_service) {
	  agent[src].t_now += 1;
	  
	// ノードのエージェント排出限界数を超えていない場合は続行
	} else {
	  node[node_position].service_num += 1;
	  node[node_position].queue -= 1;
	  
	  // 最短経路上の次ステップを探索する
	  next_position = dijkstra(node_num, link_num, node, link, max_resource, gamma, agent, src);
	  
	  /*--- 移動できない場合 ---*/
	  if (next_position == node_position) {
	    
	    // 待機
	    agent[src].t_now += 1;
	    agent[src].soc -= 1;
	    node[next_position].queue += 1;
	    
	  /*--- 移動できない場合 ---*/
	  } else if (node[next_position].queue >= max_resource) {

	    // 待機
	    agent[src].t_now += 1;
	    agent[src].soc -= 1;
	    node[node_position].queue += 1;
	    
	  /*--- 移動できる場合 ---*/
	  } else {
	    agent[src].current = next_position;
	    agent[src].hop += 1;
	    agent[src].soc -= 1;
	    
	    // 目的地に到達したら消滅
	    if (next_position == agent[src].dest) {
	      agent[src].t_now = -1;
	      agent[src].t_out = t;
	      agent[src].eval = 1;
	      success += 1;
	      
	    // 目的地でない場合は場所をupdate
	    } else {
	      agent[src].t_queue_in = t;
	      agent[src].t_now += 1;
	      node[next_position].queue += 1;
	    }
	  }
	}
      }
    }
  }

  // update
  info.success = success;

  return info;
}

void node_updating_process(int node_num, node_t node[], int max_resource) {
  int i;
  
  for (i = 0; i < node_num; i++) {
    
    // ノードのエージェント排出限界数をデフォルトに戻す
    node[i].service_num = 0;
    
    // マイナスになったキューサイズを0に戻す
    if (node[i].queue < 0) {
      printf("%d ... %d\n", i, node[i].queue);
      printf("###ERROR### キューがマイナスになっています\n");
      exit(1);
      
    // バッファを超えたエージェントを削除する
    } else if (max_resource < node[i].queue) {
      printf("%d ... %d < %d\n", i, max_resource, node[i].queue);
      printf("###ERROR### キューが最大値を超えています\n");
      exit(1);
    }
  }
}

void output_result(agent_t agent[], int ini_agent_num, int next_agent_id, node_t node[], int node_num, infra_t infra[], int t, int max_resource, int max_soc, double lambda, int max_service, double gamma, char *OUT_DIR) {
  int i;
  char filename[10000], filename2[10000], filename3[10000];
  FILE *fp, *fp2, *fp3;
  
  // 出力ファイル ... エージェント情報
  sprintf(filename, "%s/agent_step_%d_%d_%d_%d_%1.9f_%d_%1.3f.csv", OUT_DIR, t, ini_agent_num, max_resource, max_soc, lambda, max_service, gamma);
  if ((fp = fopen(filename, "w")) == NULL) {
    printf("###ERROR### 出力ファイル（リンク）を開けませんでした\n");
    exit(1);
  }
  for (i = 0; i < next_agent_id; i++) {
    fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%1.3f_%d\n", i, agent[i].t_in, agent[i].t_now, agent[i].t_out, agent[i].origin, agent[i].dest, agent[i].current, agent[i].hop, agent[i].eval, agent[i].soc, agent[i].soc_limit, agent[i].charging_flag);
  }
  fclose(fp);
  
  // 出力ファイル ... ノード情報
  sprintf(filename2, "%s/node_step_%d_%d_%d_%d_%1.9f_%d_%1.3f.csv", OUT_DIR, t, ini_agent_num, max_resource, max_soc, lambda, max_service, gamma);
  if ((fp2 = fopen(filename2, "w")) == NULL) {
    printf("###ERROR### 出力ファイル（ノード）を開けませんでした\n");
    exit(1);
  }
  for (i = 0; i < node_num; i++) {
    fprintf(fp2, "%d,%d,%d,%d,%d,%d\n", i, node[i].layer, node[i].queue, node[i].all_charger_num, node[i].used_charger_num, node[i].history);
  }
  fclose(fp2);
  
  // 出力ファイル ... 充電可否情報報
  sprintf(filename3, "%s/infra_step_%d_%d_%d_%d_%1.9f_%d_%1.3f.csv", OUT_DIR, t, ini_agent_num, max_resource, max_soc, lambda, max_service, gamma);
  if ((fp3 = fopen(filename3, "w")) == NULL) {
    printf("###ERROR### 出力ファイル（充電可否）を開けませんでした\n");
    exit(1);
  }
  for (i = 0; i < t; i++) {
    fprintf(fp3, "%d,%d,%d,%d,%d\n", i, infra[i].failure, infra[i].new, infra[i].charging, infra[i].skip);
  }
  fclose(fp3);
}
