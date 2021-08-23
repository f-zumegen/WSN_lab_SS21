// Dijkstra's Algorithm in C

#include <stdio.h>
#define INFINITY 9999
#define MAX 13

void Dijkstra(int Graph[MAX][MAX], int n, int start);

void Dijkstra(int Graph[MAX][MAX], int n, int start) {
  int cost[MAX][MAX], distance[MAX], pred[MAX];
  int visited[MAX], count, mindistance, nextnode, i, j;

  // Creating cost matrix
  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++)
      if (Graph[i][j] == 0)
        cost[i][j] = INFINITY;
      else
        cost[i][j] = Graph[i][j];

  for (i = 0; i < n; i++) {
    distance[i] = cost[start][i];
    pred[i] = start;
    visited[i] = 0;
  }

  distance[start] = 0;
  visited[start] = 1;
  count = 1;

  while (count < n - 1) {
    mindistance = INFINITY;

    for (i = 0; i < n; i++)
      if (distance[i] < mindistance && !visited[i]) {
        mindistance = distance[i];
        nextnode = i;
      }

    visited[nextnode] = 1;
    for (i = 0; i < n; i++)
      if (!visited[i])
        if (mindistance + cost[nextnode][i] < distance[i]) {
          distance[i] = mindistance + cost[nextnode][i];
          pred[i] = nextnode;
        }
    count++;
  }

  // Printing the distance
  for (i = 0; i < n; i++)
    if (i != start) {
      printf("\nDistance from source %d to %d: %d\n", start, i, distance[i]);
			printf("Path= %d",i);
			j=i;
			do{
				j=pred[j];
				printf(" <- %d",j);
			}while(j!=start);
    }
}
int main() {
  int Graph[MAX][MAX], i, j, n, u;
  n = MAX;

  for(i=0;i<MAX;i++){
		for(j=0;j<MAX;j++){
			Graph[i][j] = 0;
		}
	}
	Graph[11][3] = 1;
  Graph[11][5] = 1;
	Graph[3][1] = 1;
	Graph[5][1] = 1;

  u = 11;
  Dijkstra(Graph, n, u);

  return 0;
}
