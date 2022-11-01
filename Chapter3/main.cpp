//ウィンドウ表示＆DirectX初期化
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<vector>
#include<string>
#ifdef _DEBUG
#include<iostream>
#endif


#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

///@brief コンソール画面にフォーマット付き文字列を表示
///@param format フォーマット(%dとか%fとかの)
///@param 可変長引数
///@remarksこの関数はデバッグ用です。デバッグ時にしか動作しません
void DebugOutputFormatString(const char* format , ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif
}

//面倒ですが、ウィンドウプロシージャは必須なので書いておきます
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {//ウィンドウが破棄されたら呼ばれます
		PostQuitMessage(0);//OSに対して「もうこのアプリは終わるんや」と伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);//規定の処理を行う
}

const unsigned int window_width=1280;
const unsigned int window_height=720;

// 3.2.2
IDXGIFactory6*   dxgiFactory_ = nullptr;
ID3D12Device*    dev_         = nullptr;
IDXGISwapChain4* swapchain_   = nullptr;
// 3.3
ID3D12CommandAllocator*    cmdAllocator_ = nullptr; // (DX12)コマンドリストを使うのに必要
ID3D12GraphicsCommandList* cmdList_      = nullptr; // (DX12)↑とこれはデバイスやIDXGIFactoryに生成後に生成する
ID3D12CommandQueue*        cmdQueue_     = nullptr; // (DX12)

void EnableDebugLayer(){
	ID3D12Debug* debugLayer=nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}


///////////////////////////////////////////////////////////
//                 エントリーポイント                    //
///////////////////////////////////////////////////////////
#ifdef _DEBUG
int main() {
#else
#include<Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif
	DebugOutputFormatString( "Show window test.");
	HINSTANCE hInst = GetModuleHandle(nullptr);

	//ウィンドウクラス生成＆登録
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;//コールバック関数の指定
	w.lpszClassName = _T("DirectXTest");     //アプリケーションクラス名(適当でいいです)
	w.hInstance = GetModuleHandle(0);        //ハンドルの取得
	RegisterClassEx(&w);                     //アプリケーションクラス(こういうの作るからよろしくってOSに予告する)

	RECT wrc = { 0,0, window_width, window_height};    //ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);//ウィンドウのサイズはちょっと面倒なので関数を使って補正する
	
//ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(
        w.lpszClassName,      //クラス名指定
		_T("DX12テスト"),     //タイトルバーの文字
		WS_OVERLAPPEDWINDOW,  //タイトルバーと境界線があるウィンドウです
		CW_USEDEFAULT,        //表示X座標はOSにお任せします
		CW_USEDEFAULT,        //表示Y座標はOSにお任せします
		wrc.right - wrc.left, //ウィンドウ幅
		wrc.bottom - wrc.top, //ウィンドウ高
		nullptr,              //親ウィンドウハンドル
		nullptr,              //メニューハンドル
		w.hInstance,          //呼び出しアプリケーションハンドル
		nullptr);             //追加パラメータ

#ifdef _DEBUG
	//デバッグレイヤーをオンに
	//デバイス生成時前にやっておかないと、デバイス生成後にやると
	//デバイスがロスとしてしまうので注意
	EnableDebugLayer();
#endif
	//DirectX12まわり初期化
	//フィーチャレベル列挙
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	HRESULT result = S_OK;
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory_)))) {
		if(FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory_)))) {
			return -1;
		}
	}
	std::vector <IDXGIAdapter*> adapters; // アダプターの列挙用
	IDXGIAdapter* tmpAdapter = nullptr;   // ここに特定の名前を持つアダプターオブジェクトが入る
	for (int i = 0; dxgiFactory_->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(tmpAdapter);
	}
	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // アダプターの説明オブジェクト取得
		std::wstring strDesc = adesc.Description;

        // 探したいアダプターの名前を確認
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	//Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&dev_)) == S_OK) {
			featureLevel = l;
			break;
		}
	}


    // ※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※
    // ※ コマンドリストとコマンドアロケーターの生成（デバイスやIDXGIFactoryの生成の後に行う）
    // ※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※※
	result = dev_->CreateCommandAllocator ( // (DX12) これで命令のメモリ領域確保
        D3D12_COMMAND_LIST_TYPE_DIRECT,     // _In_         D3D12_COMMAND_LIST_TYPE type
        IID_PPV_ARGS(& cmdAllocator_)       //              REFIID                  riid
    );                                      // _COM_Outptr_ void **                 ppCommandAllocator(IID_PPV_ARGSが展開されてこの引数に渡されている)
	
    result = dev_->CreateCommandList      ( // (DX12) これに命令を溜めておいて後にコマンドキューで一気に実行する
        0,                                  // _In_         UINT                    nodeMask,
        D3D12_COMMAND_LIST_TYPE_DIRECT,     // _In_         D3D12_COMMAND_LIST_TYPE type,
        cmdAllocator_,                      // _In_         ID3D12CommandAllocator* pCommandAllocator,
        nullptr,                            // _In_opt_     ID3D12PipelineState *   pInitialState,
        IID_PPV_ARGS(&cmdList_)             //              REFIID                  riid,
    );                                      // _COM_Outptr_ void **                 ppCommandList(IID_PPV_ARGSが展開されてこの引数に渡されている)
	//_cmdList->Close();
    
    
    // コマンドキュー設定＆作成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags     = D3D12_COMMAND_QUEUE_FLAG_NONE;                                     // タイムアウトなし
	cmdQueueDesc.NodeMask  = 0;                                                                 // アダプターを１つしか使わないときは０でよい
	cmdQueueDesc.Priority  = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;                               // プライオリティ特に指定なし
	cmdQueueDesc.Type      = D3D12_COMMAND_LIST_TYPE_DIRECT;                                    // ここはコマンドリストと合わせてください
	result                 = dev_->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue_)); // コマンドキュー生成

    //コマンドリスト、コマンドアロケータ回りの気になったツイート
#if 0
    /*
    https://twitter.com/tn_mai/status/781898621276925952
    コマンドアロケータを複数のコマンドリストに関連付けるには、
    コマンドリストをクローズしてから次のコマンドリストに割り当てればよい

    https://twitter.com/Takupy256/status/785332772285980672
    Direct3D12 のコマンドアロケータってレンダーターゲットの
    バッファの分だけ作る必要があるのかよくわからないなぁ。
    サンプルでは大半は作ってないけど、作ってるのも 2 つくらいある。

    https://twitter.com/nozomi_hiragi/status/1043939158585012224
    コマンドリストはスレッド数分
    コマンドキューは多分一個でいい
    コマンドアロケータはスレッドxバックバッファ数だと思う
    */
#endif


    // スワップチェーン設定＆作成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {}; // DXGI_SWAP_CHAIN_DESCではD3D11CreateDeviceAndSwapChain()に渡していた
	swapchainDesc.Width              = window_width + 200;
	swapchainDesc.Height             = window_height;
	swapchainDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo             = false;
	swapchainDesc.SampleDesc.Count   = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage        = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount        = 2;
	swapchainDesc.Scaling            = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	result = dxgiFactory_->CreateSwapChainForHwnd(
        cmdQueue_,                     // /* [annotation][in]  */ _In_           IUnknown*                        pDevice,           コマンドキューオブジェクト
		hwnd,                          // /* [annotation][in]  */ _In_           HWND                             hWnd,              ウィンドウハンドル
		&swapchainDesc,                // /* [annotation][in]  */ _In_     const DXGI_SWAP_CHAIN_DESC1*           pDesc,             スワップチェーン設定
		nullptr,                       // /* [annotation][in]  */ _In_opt_ const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,   ひとまず nullptr でよい
		nullptr,                       // /* [annotation][in]  */ _In_opt_       IDXGIOutput*                     pRestrictToOutput, 同様
		(IDXGISwapChain1**)&swapchain_ // /* [annotation][out] */ _COM_Outptr_   IDXGISwapChain1**                ppSwapChain        スワップチェーンオブジェクト取得用
    );

    // ディスクリプタヒープ設定＆生成 (DX12)
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;  // D3D12_DESCRIPTOR_HEAP_TYPE Type;   レンダーターゲットビューなので当然RTV
	heapDesc.NodeMask       = 0;                               // UINT NumDescriptors;               複数GPUがある場合に識別を行うためのビットフラグ
	heapDesc.NumDescriptors = 2;                               // D3D12_DESCRIPTOR_HEAP_FLAGS Flags; ディスクリプタの数。表裏画面の２つ
	heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // UINT NodeMask;                     ビューにあたる情報をシェーダ側から参照する必要の有無の指定。今回はシェーダーから見える
                                                               //                                    必用がないため D3D12_DESCRIPTOR_HEAP_FLAG_NONE。

	ID3D12DescriptorHeap* rtvHeaps = nullptr; // ここにディスクリプタヒープ生成
	result                         = dev_->CreateDescriptorHeap(&heapDesc /*DESC 設定*/, IID_PPV_ARGS(&rtvHeaps) /*ポインタを渡して関数に生成を委ねる*/);
	DXGI_SWAP_CHAIN_DESC swcDesc   = {};
	result                         = swapchain_->GetDesc(&swcDesc);

    // ディスクリプタとスワップチェーン上のバッファとの関連付け処理
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);                           // スワップチェーンのバッファ
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();      // ディスクリプタヒープ上のビューのアドレスを取得
	for (size_t i = 0; i < swcDesc.BufferCount; ++i) {
		result = swapchain_->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&_backBuffers[i])); // スワップチェーン上のバックバッファ取得
		dev_->CreateRenderTargetView(_backBuffers[i], nullptr, handle);                       // RTV生成。バックバッファとディスクリプタが関連付けられる
		handle.ptr += dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // ディスクリプタ内のメモリ領域はビューなどによってずらすメモリ幅が違うためこの関数でその幅を取得してずらす。
                                                                                              // 今回はレンダーターゲットビューを表すD3D12_DESCRIPTOR_HEAP_TYPE_RTVを引数に渡す
	}

    // コマンドリストの全ての命令を待つオブジェクト
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal    = 0;
	result              = dev_->CreateFence(
        _fenceVal,             //              UINT64            InitialValue,
        D3D12_FENCE_FLAG_NONE, //              D3D12_FENCE_FLAGS Flags,
        IID_PPV_ARGS(&_fence)  //              REFIID            riid,
    );                         // _COM_Outptr_ void**            ppFence

	ShowWindow(hwnd, SW_SHOW);//ウィンドウ表示

	MSG msg = {};
	while (true) {

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {			
			break;
		}
		
		
		//DirectX処理
        /*(これだけでは問題あり)
            1. コマンドアロケーターとコマンドリストをクリア
            2. 【コマンド】レンダーターゲットをバックバッファにセット
            3. 【コマンド】レンダーターゲットを指定色でクリア
            4. 【コマンド】レンダーターゲットのクローズ
            5. 溜まったコマンドをコマンドリストに投げる
            6. スワップチェーンのフリップ処理（Present)
        */
		//バックバッファのインデックスを取得
		auto bbIdx = swapchain_->GetCurrentBackBufferIndex();

        // 描画開始時のバリア
		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource   = _backBuffers[bbIdx];
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		BarrierDesc.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
		cmdList_->ResourceBarrier(1, &BarrierDesc);

		//レンダーターゲットを指定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<ULONG_PTR>(bbIdx * dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		cmdList_->OMSetRenderTargets( // // (2.) 取得したインデックスで示されるRTVをこれから利用するものとしてセット
            1,      // _In_           UINT                         NumRenderTargetDescriptors,       レンダーターゲット数（１でよい）
            &rtvH,  // _In_opt_ const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,         レンダーターゲットハンドルの先頭アドレス
            false,  // _In_           BOOL                         RTsSingleHandleToDescriptorRange, 複数時に連足しているか
            nullptr // _In_opt_ const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor           深度ステンシルバッファビューのハンドル（nullptrでよい）
        );
		//画面クリア
		float clearColor[] = {1.0f,1.0f,0.0f,1.0f};//黄色
		cmdList_->ClearRenderTargetView(rtvH, clearColor, 0, nullptr); // (3.)

        // 描画終了時のバリア
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		BarrierDesc.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
		cmdList_->ResourceBarrier(1, &BarrierDesc);

		//命令のクローズ
		cmdList_->Close(); // // (4.)

		
		//コマンドリストの実行
		ID3D12CommandList* cmdlists[] = { cmdList_ }; // // (5.)
		cmdQueue_->ExecuteCommandLists(1, cmdlists);
		////待ち
		cmdQueue_->Signal(_fence, ++_fenceVal);

		if(_fence->GetCompletedValue() != _fenceVal) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		cmdAllocator_->Reset();                  // (1.)キューをクリア
		cmdList_->Reset(cmdAllocator_, nullptr); // 再びコマンドリストをためる準備


		//フリップ
		swapchain_->Present(1, 0); // (6.)
		
	}
	//もうクラス使わんから登録解除してや
	UnregisterClass(w.lpszClassName, w.hInstance);	
	return 0;
}