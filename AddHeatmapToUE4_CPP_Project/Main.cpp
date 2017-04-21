// Chks SLN file selected.
// Copies WHOLE folder / sub-folders (except for selected, e.g. Debug/Release/ipch/.vs)
// All files except seleced, e.g.SDF files copied
// sln and .vcxproj files EDITED so that 'old' solution/folder/project names chngd to 'new' solution.
// other .vcxproj files RENAMED to 'new' solution.
#include <string>
using namespace std;
#include "Resource1.h"  //****for Dialogs
#include <vector>	
#include "Constants.h"
#include "timer.h"

// tell the compiler to always link into a Winmain, even if we are using a console app
#pragma comment (linker, "/ENTRY:WinMainCRTStartup") 

/*
The following code tells the linker whether to create a console or not. If we are debugging logic,
we can easily switch to console output and print debug messages to the console.
*/
// comment this out for OGL usage.
//#define _CONSOLEONLY_

#ifdef _CONSOLEONLY_
	#pragma comment (linker, "/SUBSYSTEM:CONSOLE") // Show console output in debug mode 
#else 
	#pragma comment (linker, "/SUBSYSTEM:WINDOWS") 
#endif 
// we include this here, so that the above define is taken into account.

#include "DebugPrint.h"
#include <fstream>

int msgState, numFilesRenamed, maxFilenameLen, dirLen, fontSize;	//****
char oldSolution[MAX_SOL_NAME_SIZE], oldSolMinusExt[MAX_SOL_NAME_SIZE], newSolution[MAX_SOL_NAME_SIZE];	//****
char newPath[MAX_PATH];
vector<string> feedback, successFbk, instructions, dirsToIgnore, extsToIgnore, invalidCharsInProjName, readMe;
vector<string> heatmapH, heatmapCPP;
HFONT hFont;	//**** Used to change font size
char dir[MAX_PATH];
ifstream insFile;	
SIZE  sz;	//****
char dirMsg[MAX_PATH + 26];	//get dir msg length for Horiz scrollbar

#define CLASS_NAME "Windows"	//**** from BaseGame
HWND	hWnd;	
HDC		hDC;
bool	bFullscreen;	// Whether to display in fullscreen
float m_width, m_height;	// The width and height of the display area or window
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);	//**** 
char text[MAX_PATH];	
Timer* timer;

HINSTANCE hInstance; //****needed for Dialogs
char *ptr, newFile[MAX_PATH], newF[MAX_PATH], newF2[MAX_PATH];	//**** added for renaming files
int cxChr, cyChr, cxCap;	//for text
int cyStep, cxPos = 10, cyPos = 0, nFirst, nLast;	//for vert scrollbar
int vScrlMax, hScrlMax; // scroll ranges
int vScrlPos, hScrlPos; // current scroll positions
int vInc, hInc;         // scroll increments

/////////////////////////////////////////////////////////////////////////////////////////////
void setTextMetrics(){
	TEXTMETRIC tm;
	GetTextMetrics(hDC, &tm);
	cxChr = tm.tmAveCharWidth;
	cxCap = (int)(cxChr * 3 / 2);
	cyChr = tm.tmHeight + tm.tmExternalLeading;
	SetTextAlign(hDC, TA_LEFT | TA_TOP);

	if (feedback.size() > 0){
		vScrlMax = max(0, (int)feedback.size() + 2 - (int)(m_height / cyChr));	//cast int req'd! if -ve, scrollbar won't show!
		GetTextExtentPoint32(hDC, LPSTR(dirMsg), strlen(dirMsg), &sz);
		maxFilenameLen = 0;
		for (int i = 0; i < (int)feedback.size(); i++){
			int c = GetTextExtentPoint32(hDC, LPSTR(feedback[i].c_str()), feedback[i].length(), &sz);
			if (sz.cx > maxFilenameLen)	//****
				maxFilenameLen = sz.cx;	//find biggest line in pixels
		}
		dirLen = maxFilenameLen + 2 * cxChr;
	}else{
		int s = instructions.size() + 1 - (int)(m_height / cyChr);
		vScrlMax = max(0, s);	//if -ve, scrollbar won't show!
		dirLen = 0;
		for (int i = 0; i < (int)instructions.size(); i++){
			GetTextExtentPoint32(hDC, LPSTR(instructions[i].c_str()), strlen(instructions[i].c_str()), &sz);
			if (sz.cx > dirLen)	//find longest line in pixels
				dirLen = sz.cx;
		}
	}

	vScrlPos = min(vScrlPos, vScrlMax);
	hScrlMax = max(0, 1 + (int)(dirLen - (int)(m_width)) / cxChr);	//if -ve, scrollbar won't show!
	hScrlPos = min(hScrlPos, hScrlMax);
	SetScrollRange(hWnd, SB_VERT, 0, vScrlMax, FALSE);
	SetScrollPos(hWnd, SB_VERT, vScrlPos, TRUE);
	SetScrollRange(hWnd, SB_HORZ, 0, hScrlMax, FALSE);
	SetScrollPos(hWnd, SB_HORZ, hScrlPos, TRUE);
}

void Resize(int width, int height){
	DebugOut("Resize called");
	if (height == 0)
		height = 1;
	m_width = (float)width;
	m_height = (float)height;
	setTextMetrics();
}

void loadTextFileIntoVector(const char* fileName, vector<string> &lines){
	char ln[MAX_LINE_SIZE];

	insFile.open(fileName, ios::in);
	if (insFile){
		while (!insFile.eof()){
			insFile.getline(ln, MAX_LINE_SIZE);
			lines.push_back(string(ln));
		}
	}
	insFile.close();
}

void Initialise(){
	DebugOut("Initialise being called");
	timer = new Timer();
	loadTextFileIntoVector("Instructions.txt", instructions);
	loadTextFileIntoVector("dirsToIgnore.txt", dirsToIgnore);
	loadTextFileIntoVector("extsToIgnore.txt", extsToIgnore);
	loadTextFileIntoVector("invalidCharInProjectName.txt", invalidCharsInProjName);
	loadTextFileIntoVector("README.txt", readMe);
	loadTextFileIntoVector("CreateHeatmap.h", heatmapH);
	loadTextFileIntoVector("CreateHeatmap.cpp", heatmapCPP);

	ShowWindow(hWnd, SW_MAXIMIZE); //****
	fontSize = 20;
	vScrlPos = hScrlPos = 0;	//for scroll bars
	hFont = CreateFont(fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
}

bool CreateScreen(int width, int height, int bpp, bool fullscreen){	//from BaseGame
	WNDCLASS				wc;
	PIXELFORMATDESCRIPTOR	pfd;
	RECT					WindowRect;
	unsigned int			PixelFormat;
	DWORD					dwStyle = WS_OVERLAPPEDWINDOW;

	bFullscreen = fullscreen;

	ZeroMemory(&wc, sizeof(wc));
	wc.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(NULL);
//	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hIcon = (HICON)LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));

	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	WindowRect.left = 0;
	WindowRect.right = width;
	WindowRect.top = 0;
	WindowRect.bottom = height;

	AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

	if (bFullscreen){
		DEVMODE dm;
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm);
		dm.dmSize = sizeof(DEVMODE);
		dm.dmPelsWidth = width;
		dm.dmPelsHeight = height;
		dm.dmBitsPerPel = bpp;
		ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
		dwStyle = WS_POPUP;
	}

	hWnd = CreateWindow(CLASS_NAME, "UE4 Copy C++ Project utility",
		dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT,
		WindowRect.right - WindowRect.left,
		WindowRect.bottom - WindowRect.top,
		NULL, NULL, wc.hInstance, NULL);

	ZeroMemory(&pfd, sizeof(pfd));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = bpp;
	pfd.cDepthBits = 24;		// 24 bit z buffer resolution

	hDC = GetDC(hWnd);
	if (!(PixelFormat = ChoosePixelFormat(hDC, &pfd))){
		MessageBox(hWnd, "Failed to choose pixel format.", "ERROR", MB_OK);
		return false;
	}

	if (!SetPixelFormat(hDC, PixelFormat, &pfd)){
		MessageBox(hWnd, "Failed to set pixel format.", "ERROR", MB_OK);
		return false;
	}
	SetForegroundWindow(hWnd);
	Initialise();
	Resize(width, height);

	return true;
}

void ReleaseScreen(){
	wglMakeCurrent(NULL, NULL);
	ReleaseDC(hWnd, hDC);
	if (bFullscreen)
		ChangeDisplaySettings(NULL, 0);
	DestroyWindow(hWnd);
}

void Shutdown(){
	DebugOut("Shutdown being called");
	delete timer;
	DeleteObject(hFont);
}
/////////////////////////////////////////////////////////////////////////
void errorMsg(string msg){
	feedback.push_back("\n");
	feedback.push_back(msg);
	feedback.push_back("\n");
}
// Taken from: http://stackoverflow.com/questions/3418231/replace-part-of-a-string-with-another-string
void replaceAll(string& str, const string from, const string to) {
	if (from.empty()) return;
	int start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		if(str.find(from+"GameMode",start_pos) != start_pos)	//don't change GameMode lines!
			str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}

void addRedirect(const char *oldS, const char *newS, vector <string>&lines) {
	lines.push_back("[/Script/Engine.Engine]");
	string t = "+ActiveGameNameRedirects=(OldGameName=\"/Script/" + string(oldS) + "\",NewGameName=\"/Script/" + string(newS) + "\")";
	lines.push_back(t.c_str());
	t = "+ActiveGameNameRedirects=(OldGameName=\"" + string(oldS) + "\",NewGameName=\"" + string(newS) + "\")";
	lines.push_back(t.c_str());
}

//vector <string> replaceStrInTextFile(const char *sfn, const char *oldS, const char *newS, bool chgUpperCase = false) {
void replaceStrInTextFile(const char *sfn, const char *oldS, const char *newS, vector <string>&lines, bool chgUpperCase = false) {
	char ln[MAX_LINE_SIZE];
//	vector <string> lines;
	string tmp, fn, strOldS = oldS, strNewS = newS;
	fstream solFile;
	char u1[MAX_SOL_NAME_SIZE], u2[MAX_SOL_NAME_SIZE];
	strcpy_s(u1, oldS); _strupr_s(u1);	//chg to upper case
	strcpy_s(u2, newS); _strupr_s(u2);
	bool defEngScriptEngine = false;

	solFile.open(sfn, ios::in);
	bool keys[26], axisMappingAlreadySet = false;
	fn = sfn;
	for (int i = 0; i < 26; i++) keys[i] = false;
	if (solFile) {
		while (!solFile.eof()) {	//process text file lines
			solFile.getline(ln, MAX_LINE_SIZE);
			tmp = ln; 
			replaceAll(tmp, oldS, newS);	//replace old strs to new

			string uStr = u1, uStr2 = u2;	//replace Uppercase _API line
			int pos = tmp.find(uStr + "_API"); 
			if (pos > 0 && strOldS != uStr) //chk orig oldName is NOT ALL Uppercase
				tmp.replace(pos, (uStr + "_API").length(), uStr2 + "_API");
			pos = tmp.find(strNewS + "_API");
			if (pos > 0 && strNewS != uStr2) //chk newName is NOT ALL Uppercase
				tmp.replace(pos, (strNewS+"_API").length(), uStr2 + "_API");

			lines.push_back(tmp);
			if ((int)fn.find("DefaultInput.ini") > 0 && (int)tmp.find("AxisMappings") > 0) {
				int pos = (int)tmp.find(","); pos = (int)tmp.find(",", pos+1);
				string c = tmp.substr(pos-1);	//get Key for Axis mapping
				keys[toupper(c[0]) - 'A'] = true;
			}
			if ((int)fn.find("DefaultInput.ini") > 0 && (int)tmp.find("AxisName=\"SaveHeatmap\"") > 0)
				axisMappingAlreadySet = true;
		}
		tmp = sfn;
		if ((int)tmp.find("DefaultEngine.ini") > 0) {	//add REDIRECT in ini
			addRedirect(oldS, newS, lines);
//			addRedirect("Heatmap100", newS, lines);
		}
		if ((int)tmp.find("DefaultInput.ini") > 0 && !axisMappingAlreadySet) {	//add Axis Binding
			int key = 'H' - 'A';	//start at H key axis mapping
			while (keys[key]) key++;	//find available key
			string mappedKey;
			mappedKey = char(key + 'A');  //has to be 'split-up' to work, as NO ctor takes a char
			lines.push_back("+AxisMappings=(AxisName=\"SaveHeatmap\",Key=" + mappedKey + ",Scale=1.000000)");
		}
	}
	solFile.close();
//	return lines;
}

void outputTxtToFile(const char *s, const std::vector<std::string> &lines) {
	fstream solFile;
	solFile.open(s, ios::out);	//output with 'new' solution
	solFile.clear();
	for (int i = 0; i < (int)lines.size(); i++)
		solFile << lines[i] << "\n";
	solFile.close();
}

//
// Rename text file & replace all oldText to newText
void processTextFile(const char *newD, const char *sfn, const char *newFile, const char *oldText, const char *newText, bool chgUpperCase=false){
	char s[MAX_PATH];
////	fstream solFile;
//
	string t = (string)newD + "\\" + (string)sfn;
//	vector <string> lines = replaceStrInTextFile(t.c_str(), oldText, newText, chgUpperCase);
	vector <string> lines;
	replaceStrInTextFile(t.c_str(), oldText, newText, lines, chgUpperCase);
	string tmp = sfn;
	replaceAll(tmp, oldText, newText);	// replace old sol strs to new in filename

	if ((int)tmp.find("GameMode") < 0)	//don't change GameMode files! 
		tmp = (string)newD + + "\\" + tmp;	//****
	else
		tmp = (string)newD + +"\\" + string(sfn);
	strcpy_s(s, tmp.c_str());

	if (MoveFile(t.c_str(), s)) 	//rename old sln/uproject to new
		outputTxtToFile(s, lines);
	lines.clear();
}

bool isInvalidCharInProjectName(string fileNameMinusExt) {
	string invalidChars = invalidCharsInProjName[0];
	for (int i = 0; i < (int)invalidChars.length(); i++) {
		if ((int)fileNameMinusExt.find(invalidChars[i]) >= 0) {	//chk invalid char
			//			DebugOut(fileNameMinusExt.c_str());
			return true;
		}
	}
	return false;
}

bool isDirToIgnore(WIN32_FIND_DATA &fileInfo) {
	for (int i = 0; i < (int)dirsToIgnore.size(); i++) {
		if (dirsToIgnore[i].length() > 0 && strcmp(fileInfo.cFileName, dirsToIgnore[i].c_str()) == 0) {
//			DebugOut(dirsToIgnore[i].c_str());
			return true;
		}
	}
	return false;
}
bool isExtToIgnore(const string fName) {
	for (int i = 0; i < (int)extsToIgnore.size(); i++) {
		if (extsToIgnore[i].length() > 0 && (int)fName.find(extsToIgnore[i]) >= 0)
				return true;
	}
	return false;
}

void renameOldToNewFile(const char *fn, const string oldFN, const string newFN) {
	string tmp = fn;
	replaceAll(tmp, oldFN, newFN);	//****replace old file to new
	char s[MAX_PATH];
	strcpy_s(s, tmp.c_str());
	MoveFile(fn, s);		//****
}

void processFiles(char currDir[], char* newDir, string lvl){
	lvl += "  ";
	HANDLE h;
	WIN32_FIND_DATA fileInfo;
	char f[MAX_PATH];
	vector<WIN32_FIND_DATA> files, dirs;
	string s;

	files.clear();
	dirs.clear();

	h = FindFirstFile("*.*", &fileInfo);
	if (h != INVALID_HANDLE_VALUE){
		do {
			if (!(strcmp(fileInfo.cFileName, ".") == 0 || strcmp(fileInfo.cFileName, "..") == 0)){
				if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){	//dir
					if (!isDirToIgnore(fileInfo)) 	
						dirs.push_back(fileInfo);	//save sub-dirs
				}
				else{	//file
					string tmp = fileInfo.cFileName;
					if(!isExtToIgnore(tmp)){
						files.push_back(fileInfo);
//						successFbk.push_back((string)fileInfo.cFileName);	//**** too much detail?
						sprintf_s(f, "%s\\%s", newDir, fileInfo.cFileName);
						if (CopyFile(fileInfo.cFileName, f, true)){
							if (tmp == oldSolution) {	//for solution file
								s = (string)newSolution + ".sln";
								processTextFile(newDir, fileInfo.cFileName, s.c_str(), oldSolMinusExt, newSolution);
							}
							int pos = tmp.find(".uproject");
							if (pos > 0) {				//for uproject files
								if (tmp == (string)oldSolMinusExt + ".uproject") {
									processTextFile(newDir, fileInfo.cFileName, tmp.c_str(), oldSolMinusExt, newSolution);
								}
							}
							if ((int)tmp.find(".cpp") > 0 || (int)tmp.find(".h") > 0 || (int)tmp.find(".cs") > 0 || (int)tmp.find(".ini") > 0) {
								processTextFile(newDir, fileInfo.cFileName, tmp.c_str(), oldSolMinusExt, newSolution, true);	//replace filename and Text inside
							}
							if (tmp == (string)oldSolMinusExt + ".png")	//replace filename only
								renameOldToNewFile(f, tmp, (string)newSolution + ".png");
						}
					}
				}
			}
		} while (FindNextFile(h, &fileInfo));
		if (GetLastError() != ERROR_NO_MORE_FILES) 
			errorMsg("Error: Can't process file: " + string(fileInfo.cFileName));
		FindClose(h);
	}else 
		errorMsg("Error: Can't process directory" + string(currDir));

	//**** work thro' dirs
	char newroot[MAX_PATH], newCopyRoot[MAX_PATH];
	for (int i = 0; i < (int)dirs.size(); i++) {
//		strcpy(currDir, t);	//****
		sprintf_s(newroot, "%s\\%s", currDir, dirs[i].cFileName);
		if (strcmp(dirs[i].cFileName, oldSolMinusExt) == 0)	//**** chg sub-dir with same name as sln
			sprintf_s(newCopyRoot, "%s\\%s", newDir, newSolution);
		else
			sprintf_s(newCopyRoot, "%s\\%s", newDir, dirs[i].cFileName);

		CreateDirectory(newCopyRoot, NULL);
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			errorMsg("Error: Output Sub-Directory Already exists!");
		else{
			SetCurrentDirectory(newroot);
			successFbk.push_back(lvl + "Sub-dir: " + (string)dirs[i].cFileName + " copied");
			processFiles(newroot, newCopyRoot, lvl);	//recursively call sub-folders
			SetCurrentDirectory("..");
		}
	}
}

void selectFiles(string opt){
	OPENFILENAME ofn;       // common dialog box structure
	HWND hwnd = GetActiveWindow();  //****            // owner window

	const int c_cMaxFiles = MAX_NUM_FILES;	//****
	const int c_cbBuffSize = (c_cMaxFiles * (MAX_PATH + 1)) + 1;
	char buffer[c_cbBuffSize];	//**** using char NOT wide char strings!

	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = &buffer[0];	//****
// Set lpstrFile[0] to '\0' so that GetOpenFileName does not use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
//	ofn.nMaxFile = sizeof(szFile);
	ofn.nMaxFile = c_cbBuffSize;	//****
	ofn.lpstrFilter = "All\0*.*\0CPP\0*.cpp\0";	//****
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;//**** multiselect REMOVED
	feedback.clear();	//****clear fbk vectors
	successFbk.clear();

	if (GetOpenFileNameA(&ofn) == TRUE){	// Display the File Open dialog box. 
		ptr = ofn.lpstrFile; //char *dir;
		ptr[ofn.nFileOffset - 1] = 0;
//		dir = ptr;	//get directory
		strcpy_s(dir, ptr);	//get directory
		//DebugOut(ptr);
		ptr += ofn.nFileOffset;
		numFilesRenamed = maxFilenameLen = 0;	//*** req'd???
		strcpy_s(oldSolution, ptr);
		sprintf_s(dirMsg, "Copying solution: %s\\%s", dir, oldSolution);
		feedback.push_back(string(dirMsg));	//**** get Solution filename
		successFbk.push_back(string(dirMsg));
	}
	DWORD err = CommDlgExtendedError();	//chk for FileOpen dialog error
	vScrlMax = numFilesRenamed;	//****for scroll bar
	setTextMetrics();
}

void createREADME(const char *fullPath) {
	char np[MAX_PATH]; 
	sprintf_s(np, "%s\\README.txt", fullPath);
	if (CreateFile(np, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) {
		outputTxtToFile(np, readMe);
		readMe.clear();
	}
}

void createHeatmapH(const char *fullPath, const char *newPrjName) {
	char np[MAX_PATH];
	char u1[MAX_SOL_NAME_SIZE];
	strcpy_s(u1, newPrjName); _strupr_s(u1);	//chg to upper case

	for (int i = 0; i < (int)heatmapH.size(); i++)
		replaceAll(heatmapH[i], "HEATMAP100", u1);

	sprintf_s(np, "%s\\Source\\%s\\CreateHeatmap.h", fullPath, newPrjName);
	if (CreateFile(np, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) {
		outputTxtToFile(np, heatmapH);
		heatmapH.clear();
	}
}

void createHeatmapCPP(const char *fullPath, const char *newPrjName) {
	char np[MAX_PATH];
	for (int i = 0; i < (int)heatmapCPP.size(); i++)
		replaceAll(heatmapCPP[i], "Heatmap100", newPrjName);

	sprintf_s(np, "%s\\Source\\%s\\CreateHeatmap.cpp", fullPath, newPrjName);
	if (CreateFile(np, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) {
		outputTxtToFile(np, heatmapCPP);
		heatmapCPP.clear();
	}
}


LRESULT CALLBACK DlgProc(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM lParam){  //for Msg TO add Dialog!
	HCURSOR hc = GetCursor();	//for reloading std cursor
	string defSol = "Heatmap_" + (string)oldSolMinusExt;
	switch(Msg)	{
		case WM_INITDIALOG:
//			SetDlgItemText(hWndDlg, IDC_NEW_SOLUTION, "CopyUE4Solution");	//default values
			SetDlgItemText(hWndDlg, IDC_NEW_SOLUTION, (LPCSTR)defSol.c_str());	//default values
			SetDlgItemText(hWndDlg, IDC_NEW_PATH, "C:\\temp");
			return TRUE;

		case WM_COMMAND:
			switch(wParam){
				case IDOK:
					GetDlgItemText(hWndDlg, IDC_NEW_SOLUTION, newSolution, MAX_SOL_NAME_SIZE);	//get new name
					GetDlgItemText(hWndDlg, IDC_NEW_PATH, newPath, MAX_PATH);	//get Path for File Open Dialog
					EndDialog(hWndDlg, 0);

					SetCursor(LoadCursor(NULL, IDC_WAIT));	//load timer cursor
//					char f[MAX_PATH], t[MAX_PATH];
					char f[MAX_PATH];
					sprintf_s(f, "%s\\%s", newPath, newSolution);
					if (isInvalidCharInProjectName(newSolution)) {
						sprintf_s(f, "Error: NEW Project name: %s has an INVALID character!", newSolution);
						errorMsg(f);
					}else {
						CreateDirectory(f, NULL);
						if (GetLastError() == ERROR_ALREADY_EXISTS)
							errorMsg("Error: Output Directory Already exists!");
						else {
							createREADME(f);
//							strcpy_s(t, dir);	//currDir 'loses' string!!! ADDED for Debug version!
							processFiles(dir, f, "");
							createHeatmapH(f, newSolution);
							createHeatmapCPP(f, newSolution);
//							strcpy(dir, t);	//**** RESTORE dir!!!
							successFbk.push_back("Copying completed. Check " + (string)f);
							feedback.clear();
							for (int i = 0; i < (int)successFbk.size(); i++)
								feedback.push_back(successFbk[i]);
							setTextMetrics();
						}
					}
					SetCursor(hc);

					InvalidateRect(hWnd, NULL, true);	//force Repaint
					return TRUE;
				case IDCANCEL:
					EndDialog(hWndDlg, 0);
					return false;
				}
				break;
	}
	return FALSE;
}

void outputIncHorizScrlPos(const char *txt, int x, int y){
	if (hScrlPos > 0){
		string t = txt;
		if (hScrlPos < (int)t.length()){
			string t1 = t.substr(hScrlPos, t.length() - hScrlPos);
			TextOut(hDC, x, y, t1.c_str(), t1.length());
		}
	}else
		TextOut(hDC, x, y, txt, strlen(txt));
}

void outputText(HWND hWnd){	
	PAINTSTRUCT ps;
	HDC hDC = BeginPaint(hWnd, &ps);
	SetTextAlign(hDC, TA_LEFT | TA_TOP);
	
	HFONT oldHFont = (HFONT)SelectObject(hDC, hFont);
	if (feedback.size() > 0){		//output changed files
		sprintf_s(text, "Solution in Dir: %s", dir);
		outputIncHorizScrlPos(text, 1, 5);
		int numOfLines = (int)feedback.size();
		int lastFileIdx = vScrlPos + (int)(m_height / cyChr) < numOfLines ? vScrlPos + (int)(m_height / cyChr) : numOfLines;
		for (int i = vScrlPos; i < lastFileIdx; i++)	//adjust list based on scrollbar pos
				outputIncHorizScrlPos(feedback[i].c_str(), cxPos, 4 * cyChr / 2 + cyChr * ((i - vScrlPos) - cyPos));
	}else{
		int lastFileIdx = vScrlPos + (int)(m_height / cyChr) < (int)instructions.size() ? vScrlPos + (int)(m_height / cyChr) : instructions.size();
		for (int i = vScrlPos; i < lastFileIdx; i++)
			outputIncHorizScrlPos(instructions[i].c_str(), 10, 4 * cyChr / 2 + cyChr * ((i - vScrlPos) - cyPos));
	}
	EndPaint(hWnd, &ps);
	ReleaseDC(hWnd, hDC);	//****
}

bool chkSolutionFileSelected(){
	string tmp = oldSolution;	//from file dialog select
	int pos = tmp.find(".sln");
	if (pos >= 0){
		tmp = tmp.substr(0, pos);
		strcpy_s(oldSolMinusExt, tmp.c_str());	//strip sln extension
		return true;
	}else{
		errorMsg("Error: Solution file was NOT selected!");
		return false;
	}
}

void setVertScollBar(){
	vInc = max(-vScrlPos, min(vInc, vScrlMax - vScrlPos));	//recalc scroll bar positions
	if (vInc){
		vScrlPos += vInc;
		ScrollWindow(hWnd, 0, -cyChr * vInc, 0L, 0L);
		SetScrollPos(hWnd, SB_VERT, vScrlPos, TRUE);
		setTextMetrics();
		InvalidateRect(hWnd, NULL, TRUE);
	}
}

void chgFontSize(const HWND &hWnd, int fontSzInc) {
	DeleteObject(hFont);
	fontSize += fontSzInc;
	if (fontSize > 48) fontSize = 48;
	if (fontSize < 12) fontSize = 12;
	hFont = CreateFont(fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
	setTextMetrics();
	InvalidateRect(hWnd, NULL, true);
}

void selectSolutionToProcess(HWND &hWnd) {
	selectFiles("ADD");	//*** ADD NOT req'd!
	if (chkSolutionFileSelected()) {
		hInstance = GetModuleHandle(0);
		DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG2), hWnd, reinterpret_cast<DLGPROC>(DlgProc));  //open MODALLY
		ShowWindow(hWnd, SW_SHOW);
	}
	InvalidateRect(hWnd, NULL, true);	//force Repaint
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
	switch(uMsg){
		case WM_KEYDOWN:
			switch(wParam)	{
				case VK_ESCAPE:		PostQuitMessage(0);	break;
				case VK_ADD:		chgFontSize(hWnd, 2); break;
				case VK_SUBTRACT:	chgFontSize(hWnd, -2); break;
				case VK_F1:			selectSolutionToProcess(hWnd); break;
				case 'A':			MessageBox(NULL, "Written by:\nS. Manning", "About", MB_OK); break;
				default: break;
			}
			break;
		case WM_DESTROY:
		case WM_CLOSE:		PostQuitMessage(0); break;
		case WM_SIZE:		Resize(LOWORD(lParam), HIWORD(lParam));	break;
		case WM_PAINT:		outputText(hWnd); break;
		case WM_VSCROLL:
			switch (LOWORD(wParam))	{
				case SB_TOP:	vInc = -vScrlPos; break;
				case SB_BOTTOM:	vInc = vScrlMax - vScrlPos; break;
				case SB_LINEUP:	vInc = -1; break;
				case SB_LINEDOWN: vInc = 1; break;
				case SB_PAGEUP:	vInc = -max(1, (int)m_height / cyChr); break;
				case SB_PAGEDOWN: vInc = max(1, (int)m_height / cyChr); break;
				case SB_THUMBPOSITION: vInc = HIWORD(wParam) - vScrlPos; break;	//chgd to HIWORD
				case SB_THUMBTRACK:	vInc = HIWORD(wParam) - vScrlPos; break;
				default:
					vInc = 0;
			}
			setVertScollBar();
			break;
		case WM_HSCROLL:
			switch (LOWORD(wParam))	{
				case SB_TOP: hInc = -hScrlPos; break;
				case SB_BOTTOM:	hInc = hScrlMax - hScrlPos; break;
				case SB_LINEUP: hInc = -1; break;
				case SB_LINEDOWN: hInc = 1; break;
				case SB_PAGEUP:	hInc = -max(1, (int)m_width / cxChr); break;	//was -8
				case SB_PAGEDOWN: hInc = max(1, (int)m_width / cxChr); break;
				case SB_THUMBPOSITION: hInc = HIWORD(wParam) - hScrlPos; break;	//*** / cxChr added
				case SB_THUMBTRACK:	hInc = HIWORD(wParam) - hScrlPos; break;
				default:
					hInc = 0;
			}
			hInc = max(-hScrlPos, min(hInc, hScrlMax - hScrlPos));
			if (hInc){
				hScrlPos += hInc;
				ScrollWindow(hWnd, -cxChr * hInc, 0, 0L, 0L);
				SetScrollPos(hWnd, SB_HORZ, hScrlPos, TRUE);
				setTextMetrics();
				InvalidateRect(hWnd, NULL, TRUE);
			}  
			break;

		case WM_MOUSEMOVE:	break;
		case WM_LBUTTONDOWN: break;
		case WM_LBUTTONUP:	break;
		case WM_MOUSEWHEEL:	//used for vert scroll bar
			vInc = GET_WHEEL_DELTA_WPARAM(wParam) < 0 ? 1 : -1;	
			setVertScollBar();
			break;
		case WM_RBUTTONDOWN: break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


/**
This is the starting point for the application
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow){
	MSG msg;
	bool done = false;

	// don't create a screen if we are in "console output" mode
#ifndef _CONSOLEONLY_
	CreateScreen(800, 600, 32, false);
#endif
	// ===========================================================================
	//            THE MAIN GAME LOOP
	// ===========================================================================
	while(!done){
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
			if(msg.message == WM_QUIT){
				done = true;
			}else{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
// don't need to release any screens if we are using a console.
#ifndef _CONSOLEONLY_
	ReleaseScreen();
#endif

	Shutdown();
	return msg.wParam;
}