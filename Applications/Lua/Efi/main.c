/*
* min.c -- a minimal Lua interpreter
* loads stdin only with minimal error handling.
* no interaction, and no standard library, only a "print" function.
*/

#include <stdio.h>
#include <signal.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"


#include <Protocol/SimpleFileSystem.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

EFI_STATUS
LoadDataFromFile (
  CHAR16  *FileName,
  UINT8   **Data,
  UINTN   *Length
  )
{

  EFI_STATUS                       Status;
  EFI_HANDLE                       *HandleBuffer;
  UINTN                            NumberOfHandles;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFile;
  UINTN                            Index;
  EFI_FILE                         *Root, *File;
  UINT64                           Pos;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &HandleBuffer
                  );

  *Data = NULL;

  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **) &SimpleFile
                    );

    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = SimpleFile->OpenVolume (SimpleFile, &Root);

    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = Root->Open (Root, &File, FileName, EFI_FILE_MODE_READ, 0);

    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = File->SetPosition (File, (UINT64)-1);
    if (EFI_ERROR (Status)) {
      continue;
    }

    File->GetPosition (File, &Pos);
    if (EFI_ERROR (Status)) {
      continue;
    }

    File->SetPosition (File, 0);
    if (EFI_ERROR (Status)) {
      continue;
    }

    *Length = (UINTN) Pos;

    *Data = AllocatePool (*Length + 1);
    (*Data)[*Length] = '\0';

    File->Read (File, Length, *Data);
    File->Close (File);
    Root->Close (Root);

    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;

}



static int report (lua_State *L, int status) {
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
    lua_pop(L, 1);
  }
  return status;
}

static int traceback (lua_State *L) {
  if (!lua_isstring(L, 1))  /* 'message' not a string? */
    return 1;  /* keep it intact */
  //lua_getfield(L, LUA_RIDX_GLOBAS, "debug");
  lua_getglobal (L, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }
  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}

static void lstop (lua_State *L, lua_Debug *ar) {
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  luaL_error(L, "interrupted!");
}

static lua_State *globalL = NULL;

static void laction (int i) {
  signal(i, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

static int docall (lua_State *L, int narg, int clear) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, traceback);  /* push traceback function */
  lua_insert(L, base);  /* put it under chunk and args */
  signal(SIGINT, laction);
  status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
  signal(SIGINT, SIG_DFL);
  lua_remove(L, base);  /* remove traceback function */
  /* force a complete garbage collection in case of errors */
  if (status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
  return status;
}

int luaopen_edk2module (lua_State *L);
LUALIB_API int luaopen_ffi(lua_State *L);


/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
int
main (
  int   Argc,
  char  *Argv[]
  )
{
//EFI_STATUS            Status;
  lua_State             *L;
  CHAR8                 *FileName;
//UINT8                 *Data;
//UINTN                 Length;
  int                   LuaStatus;
//int                   base;

  L = luaL_newstate();
  globalL = L;

  lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
  luaL_openlibs(L);  /* open libraries */
  luaL_requiref(L, "edk2", luaopen_edk2module, 1);
  lua_pop(L, 1);  /* remove lib */
  luaL_requiref(L, "ffi", luaopen_ffi, 1);
  lua_pop(L, 1);  /* remove lib */


  lua_gc(L, LUA_GCRESTART, 0);

  FileName = Argc > 1 ? Argv[1] : "test.lua";
  LuaStatus = luaL_loadfile(L, FileName) || docall(L, 0, 1);
  if (LuaStatus) {
    report (L, LuaStatus);
  }

  lua_close(L);

  return EFI_SUCCESS;

}

