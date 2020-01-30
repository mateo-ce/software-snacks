// MCE, 2020
// WINDOWS 8+
#include <filesystem> // Basic directory functions
#include <shobjidl_core.h> // SHGetPropertyStoreFromParsingName
#include <propkey.h> // PKEY IDs
#include <vector> // Dynamic arrays
#include <algorithm> // std::find

using namespace std::filesystem;
int main(unsigned int argc, char* argv[]) {
	#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup") // Hide console
	
	const std::wstring OPT_EXT = L"MP3"; // File extension to look out for

	bool OPT_COPY_FILES = false; // Copy files, leave folders or move files, delete folders
	bool OPT_ALWAYS_SEARCH_PROPKEYS = true; // Search contributing artists even if artist isnt "Various"

	// Parse command line arguments
	for (unsigned int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "-cf") {
			OPT_COPY_FILES = true;
		} else if (std::string(argv[i]) == "-nsp") {
			OPT_ALWAYS_SEARCH_PROPKEYS = false;
		}
	}

	// Ignore featured artists in files that trigger this filter
	const std::wstring FILENAME_FILTER[] = { L"FEATURING", L"FEAT ", L"FEAT.", L"FT.", L"COVER", L"REMIX", L"EDIT", L"FLIP" };
	
	std::vector<std::wstring> parentPaths; // Store the parent paths of folders for deletion

	// Get our current directory
	TCHAR currentDirectoryBuffer[260];
	GetCurrentDirectoryW(260, currentDirectoryBuffer);
	
	std::wstring CURRENT_PATH = currentDirectoryBuffer;	// Store current directory
	CURRENT_PATH.push_back(L'\\');

	// Try to initialize a COM interface
	const HRESULT CI_HR = CoInitialize(0);
	if (FAILED(CI_HR)) return -1;

	// Recursively search all the children of our parent path, including invisible ones
	for (recursive_directory_iterator rdir(CURRENT_PATH), end; rdir != end; ++rdir) {
		if (!exists(rdir->path())) continue; // Access denied?

		wchar_t* fileExtensionUpr;
		_wcsupr_s(fileExtensionUpr = _wcsdup(rdir->path().extension().wstring().c_str()), wcslen(rdir->path().extension().wstring().c_str()) + 1);

		// Is it the extension we're looking for?
		// Ignore them if they're in the directory of the .exe
		if (!rdir->is_directory()
		&& std::wstring(fileExtensionUpr).find(L'.' + OPT_EXT) != std::string::npos
		&& rdir->path().parent_path().wstring() != CURRENT_PATH) {
			// Check that all the characters before the first space in filename are digits
			bool considerFile = true;
			unsigned int nc = 0;
			for (const wchar_t &wc : rdir->path().filename().wstring()) {
				nc++;
				if (wc == L' ') break;
				if (!isdigit(wc)) {
					considerFile = false;
					break;
				}
			}

			if (considerFile) {
				// Get the parent folder that is in the same directory as current path
				// We wouldnt need to use a fileName var, but we need it later for directory deletion
				std::wstring fileName = L"";
				for (path::iterator pdir = rdir->path().begin(); pdir != rdir->path().end(); ++pdir) {
					if (CURRENT_PATH.find(pdir->wstring()) == std::string::npos) {
						fileName = pdir->wstring();
						break;
					}
				}

				// This wstring stores the name the file will have before its first dash
				std::wstring realArtist = L"";

				// Add a call to get the file property keys, because of GPS_DELAYCREATION, this isnt called right now
				IPropertyStore* filePropertyStore = 0;
				const HRESULT SH_HR = SHGetPropertyStoreFromParsingName(rdir->path().wstring().c_str(), 0, GPS_DELAYCREATION, IID_IPropertyStore, (void**)&filePropertyStore);

				// Not sure under which circumstances SHGPSFPN can fail,
				// or if this will trigger later due to GPS_DELAYCREATION.
				// However, this is here in case it does.
				if (FAILED(SH_HR)) continue;

				PROPVARIANT albumArtist;		// PKEY_Music_AlbumArtist ('Album Artist')
				PROPVARIANT contributingArtist;	// PKEY_Music_Artist ('Contributing Artists')

				// If the artists are listed as various, search propkeys
				if (fileName == L"Various Artists" || fileName == L"Varios Artistas") {
					filePropertyStore->GetValue(PKEY_Music_AlbumArtist, &albumArtist);

					if (albumArtist.pwszVal != 0
					&& wcscmp(albumArtist.pwszVal, L"Various Artists") != 0
					&& wcscmp(albumArtist.pwszVal, L"Varios Artistas") != 0) {
						// Real artist IS IN AA
						realArtist = albumArtist.pwszVal;
					} else {
						filePropertyStore->GetValue(PKEY_Music_Artist, &contributingArtist);

						if (contributingArtist.pwszVal != 0
						&& wcscmp(contributingArtist.calpwstr.pElems[0], L"Various Artists") != 0
						&& wcscmp(contributingArtist.calpwstr.pElems[0], L"Varios Artistas") != 0) {
							// Real artist IS IN CA
							realArtist = contributingArtist.calpwstr.pElems[0];
						} else {
							// GIVE UP
							realArtist = L"Various Artists";
						}
					}
				} else {
					realArtist = fileName;
				}

				// Search property keys for extra artists to add as feat, only if:
				// 1. We havent searched contributing artists
				// 2. The file name does not contain any of the strings in a filter
				// 3. The string in contributing artists is not solely the real artist
				
				// This wstring stores the name the file will have added as (ft. *)
				std::wstring featuredArtist = L"";
				if (OPT_ALWAYS_SEARCH_PROPKEYS) PropVariantInit(&contributingArtist);

				// 1.
				if (OPT_ALWAYS_SEARCH_PROPKEYS && contributingArtist.pwszVal == 0) {
					wchar_t* fileNameUpr;
					_wcsupr_s(fileNameUpr = _wcsdup(rdir->path().filename().wstring().c_str()), wcslen(rdir->path().filename().wstring().c_str()) + 1);

					// 2.
					bool considerRename = true;
					for (const std::wstring& ws : FILENAME_FILTER) {
						if (std::wstring(fileNameUpr).find(ws) != std::string::npos) {
							considerRename = false;
							break;
						}
					}

					if (considerRename) {
						filePropertyStore->GetValue(PKEY_Music_Artist, &contributingArtist);

						if (contributingArtist.pwszVal != 0 && contributingArtist.calpwstr.pElems[0] != 0) {
							featuredArtist = contributingArtist.calpwstr.pElems[0];
							wchar_t* featuredArtistUpr;
							wchar_t* realArtistUpr;

							_wcsupr_s(featuredArtistUpr = _wcsdup(featuredArtist.c_str()), wcslen(featuredArtist.c_str()) + 1);
							_wcsupr_s(realArtistUpr = _wcsdup(realArtist.c_str()), wcslen(realArtist.c_str()) + 1);

							// Check if contributing artists has real artist in it, and remove it - Also remove any trailing commas
							const size_t FA_FIND_INDEX = std::wstring(featuredArtistUpr).find(realArtistUpr);
							if (FA_FIND_INDEX != std::string::npos) {
								unsigned int ri = 0;
								for (size_t i = wcslen(realArtist.c_str()); i < wcslen(featuredArtist.c_str()); i++) {
									if (featuredArtist.at(i) != L',' && featuredArtist.at(i) != L' ') break;
									ri++;
								}
								featuredArtist.erase(FA_FIND_INDEX, wcslen(realArtist.c_str()) + ri);
							}
							free(realArtistUpr);
							free(featuredArtistUpr);

							// 3.
							if (!featuredArtist.empty()) featuredArtist = L" (ft. " + featuredArtist + L')';
						}
					}
					free(fileNameUpr);
				}
				filePropertyStore->Release();

				std::wstring finalName = rdir->path().filename().wstring();

				finalName.replace(0, nc, realArtist + L" - ");
				finalName.insert(finalName.find(rdir->path().extension()), featuredArtist);

				// Now that we have a filename, lets copy/move it to current path
				const path FINAL_PATH = CURRENT_PATH + finalName;
				if (OPT_COPY_FILES) {
					CopyFileW(rdir->path().wstring().c_str(), FINAL_PATH.c_str(), 0);
				} else {
					const bool FM_BOOL = MoveFileW(rdir->path().wstring().c_str(), FINAL_PATH.c_str());
					if (FM_BOOL && !fileName.empty()
					&& (std::find(parentPaths.begin(), parentPaths.end(), fileName) == parentPaths.end())) {
						parentPaths.push_back(fileName);
					}
				}
			}
		}
		free(fileExtensionUpr);
	}

	// Delete all root directories of the moved files
	if (!OPT_COPY_FILES) {
		for (const std::wstring& ws : parentPaths) {
			std::wstring pathToDelete = CURRENT_PATH + ws;
			pathToDelete.push_back(L'\0');

			SHFILEOPSTRUCT FO_D = {
				0,
				0x0003, // FO_DELETE
				pathToDelete.c_str(),
				0,
				0x0010 | 0x0400 | 0x0004, // FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT
				0
			};

			SHFileOperationW(&FO_D);
		}
	}

	// Stop the COM interface
	CoUninitialize();

	return 0;
}