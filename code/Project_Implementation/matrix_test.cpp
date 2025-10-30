#include<bits/stdc++.h>

using namespace std;

int main()
{
    int K_DIM = 3;
    int M = 4;
    int N = 3;
    int A[M][K_DIM];

    // define A
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < K_DIM; j++)
        {
            A[i][j] = i*K_DIM + j + 1;
        }
    }

    int ws_clks = (M + N + K_DIM - 1);
    for (int clks = 0; clks < ws_clks; clks++)
    {
        for(int i = 0; i < K_DIM; i++) {
            if(clks - i >= 0 && clks - i < M)
                cout << A[clks-i][i] << " ";
            else
                cout << 0 << " ";
        }
        cout << endl;
    }
    return 0;
}

int main()
{
    int K_DIM = 3;
    int M = 4;
    int N = 3;

    int A_os[M][K_DIM];
    int W_os[K_DIM][N];

    int temp;
    if (M > N)
        temp = M;
    else
        temp = N;

    // define A_os
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < K_DIM; j++)
        {
            A_os[i][j] = i*K_DIM + j + 1;
        }
    }

    // define W_os
    for (int i = 0; i < K_DIM; i++) {
        for (int j = 0; j < N; j++)
        {
            W_os[i][j] = i*N + j + 1;
        }
    }

    int os_clks = (2*temp + K_DIM - 1);
    for(int clks = 0; clks < os_clks; clks++)
    {
        for(int i = 0; i < M; i++) {
            if(clks - i >= 0 && clks - i < K_DIM)
                cout << A_os[i][clks - i] << " ";
            else
                cout << 0 << " ";
        }
        for(int j = 0; j < N; j++) {
            if(clks - j >= 0 && clks - j < K_DIM)
                cout << W_os[clks - j][j] << " ";
            else
                cout << 0 << " ";
        }
        cout << endl;
    }

    return 0;
}