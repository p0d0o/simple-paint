#include <windows.h>
#include <TCHAR.H>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "resource.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK NewPaintProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
HBITMAP screenCapture(HWND hwnd);
void SaveBitmap(HBITMAP hBmp, char *path);
void OpenBitmap(HDC hdc, HBITMAP hBmp);

HINSTANCE hInst;
HWND hwndMain;
HBITMAP hBmp;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	HWND hwnd;
	MSG msg;
	WNDCLASS WndClass;
	HACCEL hAcc;
	hAcc = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
	hInst = hInstance;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
	WndClass.lpszClassName = _T("Window Class Name");
	RegisterClass(&WndClass);

	hwnd = CreateWindow(_T("Window Class Name"),
		_T("그림판"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(hwnd, hAcc, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}

static bool erase = false;

#define MAX 100

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc, pdc;
	PAINTSTRUCT ps;
	HPEN hPen, oldPen, hEraser, hSelect;
	HBRUSH hBrush, oldBrush;
	static int oldX, oldY, mx, my, sx1, sy1, sx2, sy2;
	static int flag, answer, select, copy;
	static bool draw = FALSE;
	static bool save = FALSE;
	CHOOSECOLOR penColor, brushColor;
	static COLORREF tmp[16], pColor, bColor;
	static OPENFILENAME OFN, SFN;
	static HBITMAP myBmp;
	TCHAR filter[] = _T("Every File(*.*)\0*.*\0Bitmap File\0*.bmp\0");
	TCHAR str[100], filepath[100] = _T("");
	static RECT rt;
	static HMENU hMenu, hSubMenu;

	static struct curve {
		int oldX, oldY, mx, my;
		COLORREF cvColor;
	} Curve[MAX];

	static struct line {
		int oldX, oldY, mx, my;
		COLORREF lColor;
	} Line[MAX];

	static struct rt {
		int oldX, oldY, mx, my;
		COLORREF rColor1, rColor2;
	} Rt[MAX];

	static struct circle {
		int oldX, oldY, mx, my;
		COLORREF ccColor1, ccColor2;
	} Circle[MAX];

	static int cvCount, lCount, rCount, ccCount;
	static bool cvDone, lDone, rDone, ccDone;

	switch (iMsg)
	{
	case WM_CREATE:
		flag = 0;
		bColor = RGB(255, 255, 255);
		hwndMain = hwnd;
		cvDone = FALSE;
		lDone = FALSE;
		rDone = FALSE;
		ccDone = FALSE;
		select = FALSE;
		copy = FALSE;
		hMenu = GetMenu(hwnd);
		hSubMenu = GetSubMenu(hMenu, 2);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_NEWPAINT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, NewPaintProc);
			if (erase) {
				InvalidateRgn(hwnd, NULL, TRUE);
				cvCount = 0;
				lCount = 0;
				rCount = 0;
				ccCount = 0;
			}
			break;
		case ID_SAVEPAINT:
			hBmp = screenCapture(hwnd);

			memset(&SFN, 0, sizeof(OPENFILENAME));
			SFN.lStructSize = sizeof(OPENFILENAME);
			SFN.hwndOwner = hwnd;
			SFN.lpstrFilter = filter;
			SFN.lpstrFile = filepath;
			SFN.nMaxFile = 256;
			SFN.lpstrDefExt = "bmp";
			SFN.lpstrInitialDir = _T(".");

			if (GetSaveFileName(&SFN) != 0) {
				SaveBitmap(hBmp, filepath);
				save = TRUE;
			}
			break;
		case ID_LOADPAINT:
			InvalidateRgn(hwnd, NULL, TRUE);
			cvCount = 0;
			lCount = 0;
			rCount = 0;
			ccCount = 0;

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hwnd;
			OFN.lpstrFilter = filter;
			OFN.lpstrFile = filepath;
			OFN.nMaxFile = 100;

			if (GetOpenFileName(&OFN) != 0) {
				_stprintf_s(str, "%s 파일을 열겠습니까?", OFN.lpstrFile);
				MessageBox(hwnd, str, _T("열기 선택"), MB_OK);
			}

			myBmp = (HBITMAP)LoadImage(hInst, OFN.lpstrFile, IMAGE_BITMAP,
				0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case ID_FREEDRAW:
			flag = 1;
			break;
		case ID_LINEDRAW:
			flag = 2;
			break;
		case ID_RECTDRAW:
			flag = 3;
			break;
		case ID_CIRCLEDRAW:
			flag = 4;
			break;
		case ID_ERASER:
			flag = 5;
			break;
		case ID_PCOLOR:
			for (int i = 0; i < 16; i++)
				tmp[i] = RGB(rand() % 256, rand() % 256, rand() % 256);

			memset(&penColor, 0, sizeof(CHOOSECOLOR));
			penColor.lStructSize = sizeof(CHOOSECOLOR);
			penColor.hwndOwner = hwnd;
			penColor.lpCustColors = tmp;
			penColor.Flags = CC_FULLOPEN;
			if (ChooseColor(&penColor) != 0) {
				pColor = penColor.rgbResult;
				InvalidateRgn(hwnd, NULL, FALSE);
			}
			break;
		case ID_BCOLOR:
			for (int i = 0; i < 16; i++)
				tmp[i] = RGB(rand() % 256, rand() % 256, rand() % 256);

			memset(&brushColor, 0, sizeof(CHOOSECOLOR));
			brushColor.lStructSize = sizeof(CHOOSECOLOR);
			brushColor.hwndOwner = hwnd;
			brushColor.lpCustColors = tmp;
			brushColor.Flags = CC_FULLOPEN;
			if (ChooseColor(&brushColor) != 0) {
				bColor = brushColor.rgbResult;
				InvalidateRgn(hwnd, NULL, FALSE);
			}
			break;
		case ID_UNDO:
			hdc = GetDC(hwnd);
			hPen = CreatePen(PS_SOLID, 1, pColor);
			hBrush = CreateSolidBrush(bColor);

			if (flag == 1) {
				
			}
			else if (flag == 2) {
				SetROP2(hdc, R2_WHITE);
				SelectObject(hdc, hPen);

				MoveToEx(hdc, Line[lCount - 1].mx, Line[lCount - 1].my, NULL);
				LineTo(hdc, Line[lCount - 1].oldX, Line[lCount - 1].oldY);

				lCount--;
			}
			else if (flag == 3) {
				SetROP2(hdc, R2_WHITE);
				SelectObject(hdc, hPen);
				SelectObject(hdc, hBrush);

				Rectangle(hdc, Rt[rCount - 1].oldX, Rt[rCount - 1].oldY, Rt[rCount - 1].mx, Rt[rCount - 1].my);

				rCount--;
			}
			else if (flag == 4) {
				SetROP2(hdc, R2_WHITE);
				SelectObject(hdc, hPen);
				SelectObject(hdc, hBrush);

				Ellipse(hdc, Circle[ccCount - 1].oldX, Circle[ccCount - 1].oldY, Circle[ccCount - 1].mx, Circle[ccCount - 1].my);

				ccCount--;
			}

			save = FALSE;
			ReleaseDC(hwnd, hdc);
			break;
		/*case ID_COPY:
			flag = 6;
			copy = TRUE;
			InvalidateRgn(hwnd, NULL, TRUE);
			break;
		case ID_PASTE:
			flag = 7;
			InvalidateRgn(hwnd, NULL, TRUE);
			break;*/
		case ID_EXIT:
			if (save) {
				PostQuitMessage(0);
			}
			else {
				answer = MessageBox(hwnd, _T("파일이 저장되지 않았습니다. 끝내겠습니까?"),
					_T("끝내기"), MB_YESNO);

				if (answer == IDYES)
					PostQuitMessage(0);
			}
			break;
		}
		break;
	case WM_LBUTTONDOWN:
		hdc = GetDC(hwnd);

		if (flag == 1) {
			if (cvDone)
				break;

			mx = LOWORD(lParam);
			my = HIWORD(lParam);
			draw = TRUE;
			save = FALSE;
		}
		else if (flag == 2) {
			if (lDone)
				break;

			mx = LOWORD(lParam);
			my = HIWORD(lParam);
			draw = TRUE;
			oldX = mx;
			oldY = my;
			save = FALSE;
		}
		else if (flag == 3) {
			if (rDone)
				break;

			mx = LOWORD(lParam);
			my = HIWORD(lParam);
			draw = TRUE;
			oldX = mx;
			oldY = my;

			Rectangle(hdc, oldX, oldY, mx, my);
			save = FALSE;
		}
		else if (flag == 4) {
			if (ccDone)
				break;

			mx = LOWORD(lParam);
			my = HIWORD(lParam);
			draw = TRUE;
			oldX = mx;
			oldY = my;

			Ellipse(hdc, oldX, oldY, mx, my);
			save = FALSE;
		}
		else if (flag == 5) {
			mx = LOWORD(lParam);
			my = HIWORD(lParam);
			draw = TRUE;
			save = FALSE;
		}
		else if (flag == 6) {
			hSelect = CreatePen(PS_DOT, 1, RGB(0, 0, 255));
			SetROP2(hdc, R2_NOP);
			SelectObject(hdc, hSelect);

			sx1 = LOWORD(lParam);
			sy1 = HIWORD(lParam);
			draw = TRUE;
			sx2 = sx1;
			sy2 = sy1;

			Rectangle(hdc, sx2, sy2, mx, my);
		}
		else if (flag == 7) {
			mx = LOWORD(lParam);
			my = LOWORD(lParam);
		}

		break;
	case WM_MOUSEMOVE:
		hdc = GetDC(hwnd);
		hPen = CreatePen(PS_SOLID, 1, pColor);
		oldPen = (HPEN)SelectObject(hdc, hPen);
		hBrush = CreateSolidBrush(bColor);
		oldBrush = (HBRUSH)SelectObject(hdc, hBrush);
		hEraser = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
		hSelect = CreatePen(PS_DOT, 1, RGB(0, 0, 255));

		if (flag == 1 && draw == TRUE) {
			SelectObject(hdc, hPen);

			MoveToEx(hdc, mx, my, NULL);
			mx = LOWORD(lParam);
			my = HIWORD(lParam);
			LineTo(hdc, mx, my);
		}
		else if (flag == 2 && draw == TRUE) {
			SetROP2(hdc, R2_NOTXORPEN);
			SelectObject(hdc, hPen);

			MoveToEx(hdc, mx, my, NULL);
			LineTo(hdc, oldX, oldY);
			oldX = LOWORD(lParam);
			oldY = HIWORD(lParam);
			MoveToEx(hdc, mx, my, NULL);
			LineTo(hdc, oldX, oldY);
		}
		else if (flag == 3 && draw == TRUE) {
			SetROP2(hdc, R2_NOTXORPEN);
			SelectObject(hdc, hPen);
			SelectObject(hdc, hBrush);

			Rectangle(hdc, oldX, oldY, mx, my);
			oldX = LOWORD(lParam);
			oldY = HIWORD(lParam);
			Rectangle(hdc, oldX, oldY, mx, my);
		}
		else if (flag == 4 && draw == TRUE) {
			SetROP2(hdc, R2_NOTXORPEN);
			SelectObject(hdc, hPen);
			SelectObject(hdc, hBrush);

			Ellipse(hdc, oldX, oldY, mx, my);
			oldX = LOWORD(lParam);
			oldY = HIWORD(lParam);
			Ellipse(hdc, oldX, oldY, mx, my);
		}
		else if (flag == 5 && draw == TRUE) {
			SetROP2(hdc, R2_WHITE);
			SelectObject(hdc, hEraser);

			MoveToEx(hdc, mx, my, NULL);
			mx = LOWORD(lParam);
			my = HIWORD(lParam);
			LineTo(hdc, mx, my);
		}
		else if (flag == 6 && draw == TRUE) {
			SetROP2(hdc, R2_NOTXORPEN);
			SelectObject(hdc, hSelect);

			Rectangle(hdc, sx2, sy2, sx1, sy1);
			sx2 = LOWORD(lParam);
			sy2 = HIWORD(lParam);
			Rectangle(hdc, sx2, sy2, sx1, sy1);
		}

		ReleaseDC(hwnd, hdc);
		break;
	case WM_LBUTTONUP:
		hdc = GetDC(hwnd);
		pdc = CreateCompatibleDC(hdc);

		if (flag == 1) {
			if (cvDone)
				break;

			Curve[cvCount].oldX = oldX;
			Curve[cvCount].oldY = oldY;
			Curve[cvCount].mx = mx;
			Curve[cvCount].my = my;
			Curve[cvCount].cvColor = pColor;
			cvCount++;

			if (cvCount == MAX)
				cvDone = TRUE;
		}
		else if (flag == 2) {
			if (lDone)
				break;

			Line[lCount].oldX = oldX;
			Line[lCount].oldY = oldY;
			Line[lCount].mx = mx;
			Line[lCount].my = my;
			Line[lCount].lColor = pColor;
			lCount++;

			if (lCount == MAX)
				lDone = TRUE;
		}
		else if (flag == 3) {
			if (rDone)
				break;

			Rt[rCount].oldX = oldX;
			Rt[rCount].oldY = oldY;
			Rt[rCount].mx = mx;
			Rt[rCount].my = my;
			Rt[rCount].rColor1 = pColor;
			Rt[rCount].rColor2 = bColor;
			rCount++;

			if (rCount == MAX)
				rDone = TRUE;
		}
		else if (flag == 4) {
			if (ccDone)
				break;

			Circle[ccCount].oldX = oldX;
			Circle[ccCount].oldY = oldY;
			Circle[ccCount].mx = mx;
			Circle[ccCount].my = my;
			Circle[ccCount].ccColor1 = pColor;
			Circle[ccCount].ccColor2 = bColor;
			ccCount++;

			if (ccCount == MAX)
				ccDone = TRUE;
		}
		else if (flag == 6) {
			select = TRUE;
		}
		draw = FALSE;
		break;
	case WM_PAINT:
		EnableMenuItem(hSubMenu, ID_PASTE, copy ? MF_ENABLED : MF_GRAYED);

		GetClientRect(hwnd, &rt);
		hdc = BeginPaint(hwnd, &ps);

		OpenBitmap(hdc, myBmp);
		
		for (int i = 0; i < cvCount; i++) {
			hPen = CreatePen(PS_SOLID, 1, Curve[i].cvColor);
			SelectObject(hdc, hPen);
		}

		for (int i = 0; i < lCount; i++) {
			hPen = CreatePen(PS_SOLID, 1, Line[i].lColor);
			SelectObject(hdc, hPen);
			MoveToEx(hdc, Line[i].mx, Line[i].my, NULL);
			LineTo(hdc, Line[i].oldX, Line[i].oldY);
		}

		for (int i = 0; i < rCount; i++) {
			hPen = CreatePen(PS_SOLID, 1, Rt[i].rColor1);
			SelectObject(hdc, hPen);
			hBrush = CreateSolidBrush(Rt[i].rColor2);
			SelectObject(hdc, hBrush);
			Rectangle(hdc, Rt[i].oldX, Rt[i].oldY, Rt[i].mx, Rt[i].my);
		}

		for (int i = 0; i < ccCount; i++) {
			hPen = CreatePen(PS_SOLID, 1, Circle[i].ccColor1);
			SelectObject(hdc, hPen);
			hBrush = CreateSolidBrush(Circle[i].ccColor2);
			SelectObject(hdc, hBrush);
			Ellipse(hdc, Circle[i].oldX, Circle[i].oldY, Circle[i].mx, Circle[i].my);
		}


		EndPaint(hwnd, &ps);
		break;
	case WM_SIZE:
		InvalidateRgn(hwnd, NULL, FALSE);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

BOOL CALLBACK NewPaintProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	switch (iMsg)
	{
	case WM_INITDIALOG:
		return 1;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_YES:
			erase = true;
			EndDialog(hDlg, 0);
			break;
		case ID_NO:
			erase = false;
			EndDialog(hDlg, 0);
			break;
		case IDCANCEL:
			EndDialog(hDlg, 0);
			break;
		}
		break;
	}

	return 0;
}

HBITMAP screenCapture(HWND hwnd) {
	HDC hdc, memdc;
	HBITMAP hBmp;
	RECT rt;

	GetClientRect(hwnd, &rt);

	hdc = GetDC(hwnd);
	memdc = CreateCompatibleDC(hdc);
	hBmp = CreateCompatibleBitmap(hdc, rt.right - rt.left, rt.bottom - rt.top);
	SelectObject(memdc, hBmp);

	BitBlt(memdc, 0, 0, rt.right - rt.left, rt.bottom - rt.top, hdc, rt.left, rt.top, SRCCOPY);


	DeleteDC(memdc);
	DeleteDC(hdc);

	return hBmp;
}

void SaveBitmap(HBITMAP hBmp, char *path) {
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER ih;
	BITMAP bmp;
	BITMAPINFO *pih;
	int pSize;
	HANDLE hFile;
	DWORD dwWritten, size;
	HDC hdc;

	hdc = GetDC(NULL);

	GetObject(hBmp, sizeof(BITMAP), &bmp);
	ih.biSize = sizeof(BITMAPINFOHEADER);
	ih.biWidth = bmp.bmWidth;
	ih.biHeight = bmp.bmHeight;
	ih.biPlanes = 1;
	ih.biBitCount = bmp.bmPlanes * bmp.bmBitsPixel;
	if (ih.biBitCount > 8)
		ih.biBitCount = 24;
	ih.biCompression = BI_RGB;
	ih.biSizeImage = 0;
	ih.biXPelsPerMeter = 0;
	ih.biYPelsPerMeter = 0;
	ih.biClrUsed = 0;
	ih.biClrImportant = 0;

	pSize = ((ih.biBitCount == 24 ? 0 : 1) << ih.biBitCount) * sizeof(RGBQUAD);
	pih = (BITMAPINFO *)malloc(ih.biSize + pSize);
	pih->bmiHeader = ih;

	GetDIBits(hdc, hBmp, 0, bmp.bmHeight, NULL, pih, DIB_RGB_COLORS);
	ih = pih->bmiHeader;

	if (ih.biSizeImage == 0) {
		ih.biSizeImage = ((((ih.biWidth * ih.biBitCount) + 31) & ~31) >> 3) *ih.biHeight;
	}

	size = ih.biSize + pSize + ih.biSizeImage;
	pih = (BITMAPINFO *)realloc(pih, size);

	GetDIBits(hdc, hBmp, 0, bmp.bmHeight, (PBYTE)pih + ih.biSize + pSize, pih, DIB_RGB_COLORS);
	
	fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pSize;
	fh.bfReserved1 = 0;
	fh.bfReserved2 = 0;
	fh.bfSize = size + sizeof(BITMAPFILEHEADER);
	fh.bfType = 0x4d42;

	hFile = CreateFile(path, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	WriteFile(hFile, &fh, sizeof(fh), &dwWritten, NULL);
	WriteFile(hFile, pih, size, &dwWritten, NULL);

	ReleaseDC(NULL, hdc);
	CloseHandle(hFile);
}

void OpenBitmap(HDC hdc, HBITMAP hBmp) {
	HDC memdc;
	HBITMAP oldBmp;
	BITMAP bmp;

	memdc = CreateCompatibleDC(hdc);
	oldBmp = (HBITMAP)SelectObject(memdc, hBmp);
	GetObject(hBmp, sizeof(BITMAP), &bmp);

	BitBlt(hdc, 0, 0, bmp.bmWidth, bmp.bmHeight, memdc, 0, 0, SRCCOPY);

	SelectObject(memdc, oldBmp);
	DeleteDC(memdc);
}