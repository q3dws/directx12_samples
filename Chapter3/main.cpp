//�E�B���h�E�\����DirectX������
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

///@brief �R���\�[����ʂɃt�H�[�}�b�g�t���������\��
///@param format �t�H�[�}�b�g(%d�Ƃ�%f�Ƃ���)
///@param �ϒ�����
///@remarks���̊֐��̓f�o�b�O�p�ł��B�f�o�b�O���ɂ������삵�܂���
void DebugOutputFormatString(const char* format , ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif
}

//�ʓ|�ł����A�E�B���h�E�v���V�[�W���͕K�{�Ȃ̂ŏ����Ă����܂�
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {//�E�B���h�E���j�����ꂽ��Ă΂�܂�
		PostQuitMessage(0);//OS�ɑ΂��āu�������̃A�v���͏I�����v�Ɠ`����
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);//�K��̏������s��
}

const unsigned int window_width=1280;
const unsigned int window_height=720;

// 3.2.2
IDXGIFactory6*   dxgiFactory_ = nullptr;
ID3D12Device*    dev_         = nullptr;
IDXGISwapChain4* swapchain_   = nullptr;
// 3.3
ID3D12CommandAllocator*    cmdAllocator_ = nullptr; // (DX12)�R�}���h���X�g���g���̂ɕK�v
ID3D12GraphicsCommandList* cmdList_      = nullptr; // (DX12)���Ƃ���̓f�o�C�X��IDXGIFactory�ɐ�����ɐ�������
ID3D12CommandQueue*        cmdQueue_     = nullptr; // (DX12)

void EnableDebugLayer(){
	ID3D12Debug* debugLayer=nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}


///////////////////////////////////////////////////////////
//                 �G���g���[�|�C���g                    //
///////////////////////////////////////////////////////////
#ifdef _DEBUG
int main() {
#else
#include<Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif
	DebugOutputFormatString( "Show window test.");
	HINSTANCE hInst = GetModuleHandle(nullptr);

	//�E�B���h�E�N���X�������o�^
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;//�R�[���o�b�N�֐��̎w��
	w.lpszClassName = _T("DirectXTest");     //�A�v���P�[�V�����N���X��(�K���ł����ł�)
	w.hInstance = GetModuleHandle(0);        //�n���h���̎擾
	RegisterClassEx(&w);                     //�A�v���P�[�V�����N���X(���������̍�邩���낵������OS�ɗ\������)

	RECT wrc = { 0,0, window_width, window_height};    //�E�B���h�E�T�C�Y�����߂�
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);//�E�B���h�E�̃T�C�Y�͂�����Ɩʓ|�Ȃ̂Ŋ֐����g���ĕ␳����
	
//�E�B���h�E�I�u�W�F�N�g�̐���
	HWND hwnd = CreateWindow(
        w.lpszClassName,      //�N���X���w��
		_T("DX12�e�X�g"),     //�^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW,  //�^�C�g���o�[�Ƌ��E��������E�B���h�E�ł�
		CW_USEDEFAULT,        //�\��X���W��OS�ɂ��C�����܂�
		CW_USEDEFAULT,        //�\��Y���W��OS�ɂ��C�����܂�
		wrc.right - wrc.left, //�E�B���h�E��
		wrc.bottom - wrc.top, //�E�B���h�E��
		nullptr,              //�e�E�B���h�E�n���h��
		nullptr,              //���j���[�n���h��
		w.hInstance,          //�Ăяo���A�v���P�[�V�����n���h��
		nullptr);             //�ǉ��p�����[�^

#ifdef _DEBUG
	//�f�o�b�O���C���[���I����
	//�f�o�C�X�������O�ɂ���Ă����Ȃ��ƁA�f�o�C�X������ɂ���
	//�f�o�C�X�����X�Ƃ��Ă��܂��̂Œ���
	EnableDebugLayer();
#endif
	//DirectX12�܂�菉����
	//�t�B�[�`�����x����
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
	std::vector <IDXGIAdapter*> adapters; // �A�_�v�^�[�̗񋓗p
	IDXGIAdapter* tmpAdapter = nullptr;   // �����ɓ���̖��O�����A�_�v�^�[�I�u�W�F�N�g������
	for (int i = 0; dxgiFactory_->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(tmpAdapter);
	}
	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // �A�_�v�^�[�̐����I�u�W�F�N�g�擾
		std::wstring strDesc = adesc.Description;

        // �T�������A�_�v�^�[�̖��O���m�F
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	//Direct3D�f�o�C�X�̏�����
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&dev_)) == S_OK) {
			featureLevel = l;
			break;
		}
	}


    // ����������������������������������������������������������������������������������������
    // �� �R�}���h���X�g�ƃR�}���h�A���P�[�^�[�̐����i�f�o�C�X��IDXGIFactory�̐����̌�ɍs���j
    // ����������������������������������������������������������������������������������������
	result = dev_->CreateCommandAllocator ( // (DX12) ����Ŗ��߂̃������̈�m��
        D3D12_COMMAND_LIST_TYPE_DIRECT,     // _In_         D3D12_COMMAND_LIST_TYPE type
        IID_PPV_ARGS(& cmdAllocator_)       //              REFIID                  riid
    );                                      // _COM_Outptr_ void **                 ppCommandAllocator(IID_PPV_ARGS���W�J����Ă��̈����ɓn����Ă���)
	
    result = dev_->CreateCommandList      ( // (DX12) ����ɖ��߂𗭂߂Ă����Č�ɃR�}���h�L���[�ň�C�Ɏ��s����
        0,                                  // _In_         UINT                    nodeMask,
        D3D12_COMMAND_LIST_TYPE_DIRECT,     // _In_         D3D12_COMMAND_LIST_TYPE type,
        cmdAllocator_,                      // _In_         ID3D12CommandAllocator* pCommandAllocator,
        nullptr,                            // _In_opt_     ID3D12PipelineState *   pInitialState,
        IID_PPV_ARGS(&cmdList_)             //              REFIID                  riid,
    );                                      // _COM_Outptr_ void **                 ppCommandList(IID_PPV_ARGS���W�J����Ă��̈����ɓn����Ă���)
	//_cmdList->Close();
    
    
    // �R�}���h�L���[�ݒ聕�쐬
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags     = D3D12_COMMAND_QUEUE_FLAG_NONE;                                     // �^�C���A�E�g�Ȃ�
	cmdQueueDesc.NodeMask  = 0;                                                                 // �A�_�v�^�[���P�����g��Ȃ��Ƃ��͂O�ł悢
	cmdQueueDesc.Priority  = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;                               // �v���C�I���e�B���Ɏw��Ȃ�
	cmdQueueDesc.Type      = D3D12_COMMAND_LIST_TYPE_DIRECT;                                    // �����̓R�}���h���X�g�ƍ��킹�Ă�������
	result                 = dev_->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue_)); // �R�}���h�L���[����

    //�R�}���h���X�g�A�R�}���h�A���P�[�^���̋C�ɂȂ����c�C�[�g
#if 0
    /*
    https://twitter.com/tn_mai/status/781898621276925952
    �R�}���h�A���P�[�^�𕡐��̃R�}���h���X�g�Ɋ֘A�t����ɂ́A
    �R�}���h���X�g���N���[�Y���Ă��玟�̃R�}���h���X�g�Ɋ��蓖�Ă�΂悢

    https://twitter.com/Takupy256/status/785332772285980672
    Direct3D12 �̃R�}���h�A���P�[�^���ă����_�[�^�[�Q�b�g��
    �o�b�t�@�̕��������K�v������̂��悭�킩��Ȃ��Ȃ��B
    �T���v���ł͑唼�͍���ĂȂ����ǁA����Ă�̂� 2 ���炢����B

    https://twitter.com/nozomi_hiragi/status/1043939158585012224
    �R�}���h���X�g�̓X���b�h����
    �R�}���h�L���[�͑�����ł���
    �R�}���h�A���P�[�^�̓X���b�hx�o�b�N�o�b�t�@�����Ǝv��
    */
#endif


    // �X���b�v�`�F�[���ݒ聕�쐬
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {}; // DXGI_SWAP_CHAIN_DESC�ł�D3D11CreateDeviceAndSwapChain()�ɓn���Ă���
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
        cmdQueue_,                     // /* [annotation][in]  */ _In_           IUnknown*                        pDevice,           �R�}���h�L���[�I�u�W�F�N�g
		hwnd,                          // /* [annotation][in]  */ _In_           HWND                             hWnd,              �E�B���h�E�n���h��
		&swapchainDesc,                // /* [annotation][in]  */ _In_     const DXGI_SWAP_CHAIN_DESC1*           pDesc,             �X���b�v�`�F�[���ݒ�
		nullptr,                       // /* [annotation][in]  */ _In_opt_ const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,   �ЂƂ܂� nullptr �ł悢
		nullptr,                       // /* [annotation][in]  */ _In_opt_       IDXGIOutput*                     pRestrictToOutput, ���l
		(IDXGISwapChain1**)&swapchain_ // /* [annotation][out] */ _COM_Outptr_   IDXGISwapChain1**                ppSwapChain        �X���b�v�`�F�[���I�u�W�F�N�g�擾�p
    );

    // �f�B�X�N���v�^�q�[�v�ݒ聕���� (DX12)
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;  // D3D12_DESCRIPTOR_HEAP_TYPE Type;   �����_�[�^�[�Q�b�g�r���[�Ȃ̂œ��RRTV
	heapDesc.NodeMask       = 0;                               // UINT NumDescriptors;               ����GPU������ꍇ�Ɏ��ʂ��s�����߂̃r�b�g�t���O
	heapDesc.NumDescriptors = 2;                               // D3D12_DESCRIPTOR_HEAP_FLAGS Flags; �f�B�X�N���v�^�̐��B�\����ʂ̂Q��
	heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // UINT NodeMask;                     �r���[�ɂ���������V�F�[�_������Q�Ƃ���K�v�̗L���̎w��B����̓V�F�[�_�[���猩����
                                                               //                                    �K�p���Ȃ����� D3D12_DESCRIPTOR_HEAP_FLAG_NONE�B

	ID3D12DescriptorHeap* rtvHeaps = nullptr; // �����Ƀf�B�X�N���v�^�q�[�v����
	result                         = dev_->CreateDescriptorHeap(&heapDesc /*DESC �ݒ�*/, IID_PPV_ARGS(&rtvHeaps) /*�|�C���^��n���Ċ֐��ɐ������ς˂�*/);
	DXGI_SWAP_CHAIN_DESC swcDesc   = {};
	result                         = swapchain_->GetDesc(&swcDesc);

    // �f�B�X�N���v�^�ƃX���b�v�`�F�[����̃o�b�t�@�Ƃ̊֘A�t������
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);                           // �X���b�v�`�F�[���̃o�b�t�@
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();      // �f�B�X�N���v�^�q�[�v��̃r���[�̃A�h���X���擾
	for (size_t i = 0; i < swcDesc.BufferCount; ++i) {
		result = swapchain_->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&_backBuffers[i])); // �X���b�v�`�F�[����̃o�b�N�o�b�t�@�擾
		dev_->CreateRenderTargetView(_backBuffers[i], nullptr, handle);                       // RTV�����B�o�b�N�o�b�t�@�ƃf�B�X�N���v�^���֘A�t������
		handle.ptr += dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // �f�B�X�N���v�^���̃������̈�̓r���[�Ȃǂɂ���Ă��炷�����������Ⴄ���߂��̊֐��ł��̕����擾���Ă��炷�B
                                                                                              // ����̓����_�[�^�[�Q�b�g�r���[��\��D3D12_DESCRIPTOR_HEAP_TYPE_RTV�������ɓn��
	}

    // �R�}���h���X�g�̑S�Ă̖��߂�҂I�u�W�F�N�g
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal    = 0;
	result              = dev_->CreateFence(
        _fenceVal,             //              UINT64            InitialValue,
        D3D12_FENCE_FLAG_NONE, //              D3D12_FENCE_FLAGS Flags,
        IID_PPV_ARGS(&_fence)  //              REFIID            riid,
    );                         // _COM_Outptr_ void**            ppFence

	ShowWindow(hwnd, SW_SHOW);//�E�B���h�E�\��

	MSG msg = {};
	while (true) {

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//�����A�v���P�[�V�������I�����Ď���message��WM_QUIT�ɂȂ�
		if (msg.message == WM_QUIT) {			
			break;
		}
		
		
		//DirectX����
        /*(���ꂾ���ł͖�肠��)
            1. �R�}���h�A���P�[�^�[�ƃR�}���h���X�g���N���A
            2. �y�R�}���h�z�����_�[�^�[�Q�b�g���o�b�N�o�b�t�@�ɃZ�b�g
            3. �y�R�}���h�z�����_�[�^�[�Q�b�g���w��F�ŃN���A
            4. �y�R�}���h�z�����_�[�^�[�Q�b�g�̃N���[�Y
            5. ���܂����R�}���h���R�}���h���X�g�ɓ�����
            6. �X���b�v�`�F�[���̃t���b�v�����iPresent)
        */
		//�o�b�N�o�b�t�@�̃C���f�b�N�X���擾
		auto bbIdx = swapchain_->GetCurrentBackBufferIndex();

        // �`��J�n���̃o���A
		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource   = _backBuffers[bbIdx];
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		BarrierDesc.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
		cmdList_->ResourceBarrier(1, &BarrierDesc);

		//�����_�[�^�[�Q�b�g���w��
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<ULONG_PTR>(bbIdx * dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		cmdList_->OMSetRenderTargets( // // (2.) �擾�����C���f�b�N�X�Ŏ������RTV�����ꂩ�痘�p������̂Ƃ��ăZ�b�g
            1,      // _In_           UINT                         NumRenderTargetDescriptors,       �����_�[�^�[�Q�b�g���i�P�ł悢�j
            &rtvH,  // _In_opt_ const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,         �����_�[�^�[�Q�b�g�n���h���̐擪�A�h���X
            false,  // _In_           BOOL                         RTsSingleHandleToDescriptorRange, �������ɘA�����Ă��邩
            nullptr // _In_opt_ const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor           �[�x�X�e���V���o�b�t�@�r���[�̃n���h���inullptr�ł悢�j
        );
		//��ʃN���A
		float clearColor[] = {1.0f,1.0f,0.0f,1.0f};//���F
		cmdList_->ClearRenderTargetView(rtvH, clearColor, 0, nullptr); // (3.)

        // �`��I�����̃o���A
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		BarrierDesc.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
		cmdList_->ResourceBarrier(1, &BarrierDesc);

		//���߂̃N���[�Y
		cmdList_->Close(); // // (4.)

		
		//�R�}���h���X�g�̎��s
		ID3D12CommandList* cmdlists[] = { cmdList_ }; // // (5.)
		cmdQueue_->ExecuteCommandLists(1, cmdlists);
		////�҂�
		cmdQueue_->Signal(_fence, ++_fenceVal);

		if(_fence->GetCompletedValue() != _fenceVal) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		cmdAllocator_->Reset();                  // (1.)�L���[���N���A
		cmdList_->Reset(cmdAllocator_, nullptr); // �ĂуR�}���h���X�g�����߂鏀��


		//�t���b�v
		swapchain_->Present(1, 0); // (6.)
		
	}
	//�����N���X�g��񂩂�o�^�������Ă�
	UnregisterClass(w.lpszClassName, w.hInstance);	
	return 0;
}