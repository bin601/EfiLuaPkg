#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>

typedef struct {
  LIST_ENTRY    Link;
  void          *Buffer;
} BUFFER_LIST;


struct {
  BUFFER_LIST                 CommandHistory;
  BOOLEAN                     InsertMode;           ///< Is the current typing mode insert (FALSE = overwrite).
} gViewingSettings = {
  {INITIALIZE_LIST_HEAD_VARIABLE (gViewingSettings.CommandHistory.Link), NULL},
  TRUE
};



//UINTN
//Print (
//IN CONST CHAR16  *Format,
//...
//)
///*++
//
//Routine Description:
//
//Print function for a maximum of EFI_DRIVER_LIB_MAX_PRINT_BUFFER ascii
//characters.
//
//Arguments:
//
//Format - Ascii format string see file header for more details.
//
//...    - Vararg list consumed by processing Format.
//
//Returns:
//
//Number of characters printed.
//
//--*/
//{
//UINTN   Return;
//VA_LIST Marker;
//STATIC  CHAR16  Buffer[0x1000];
//
//VA_START (Marker, Format);
//Return = UnicodeVSPrint (Buffer, sizeof (Buffer), Format, Marker);
//VA_END (Marker);
//
//if (gST->ConOut != NULL) {
//  //
//  // To be extra safe make sure ConOut has been initialized
//  //
//  gST->ConOut->OutputString (gST->ConOut, Buffer);
//}
//
//return Return;
//}


CHAR8 *
Unicode2Ascii (
  OUT CHAR8          *AsciiStr,
  IN  CHAR16         *UnicodeStr
  )
/*++

Routine Description:
  Converts Unicode to ASCII characters.
  Assumes that the Unicode characters are only these defined in the ASCII set.

Arguments:
  AsciiStr   - The ASCII string to be written to. The buffer must be large enough.
  UnicodeStr - the Unicode string to be converted.

Returns:
  The address to the ASCII string - same as AsciiStr.

--*/
{
  CHAR8 *Str;

  Str = AsciiStr;

  while (TRUE) {

    *AsciiStr = (CHAR8) *(UnicodeStr++);

    if (*(AsciiStr++) == '\0') {
      return Str;
    }
  }
}



EFI_STATUS
EFIAPI
_readline (
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  );

char *
readline (
  const char *p
  )
{
  UINTN                         BufferSize;
  STATIC CHAR16                 Buffer[0x1000];
  CHAR8                         *AsciiStr;

  BufferSize = sizeof (Buffer);
  _readline (&BufferSize, Buffer);

  AsciiStr = AllocateZeroPool (BufferSize / sizeof (CHAR16) + 1);
  Unicode2Ascii (AsciiStr, Buffer);

  return AsciiStr;
}



VOID
Ascii2UnicodeString (
  CHAR8    *String,
  CHAR16   *UniString
  )
/*++

Routine Description:

  Write ascii string as unicode string format to FILE

Arguments:

  String      - Pointer to string that is written to FILE.
  UniString   - Pointer to unicode string

Returns:

  NULL

--*/
{
  while (*String != '\0') {
    *(UniString++) = (CHAR16) *(String++);
  }
  //
  // End the UniString with a NULL.
  //
  *UniString = '\0';
}

/**
  Add a buffer to the Line History List

  @param Buffer     The line buffer to add.
**/
VOID
EFIAPI
add_history (
  IN CONST CHAR8 *Buffer
  )
{
  BUFFER_LIST *Node;

  Node = AllocateZeroPool(sizeof(BUFFER_LIST));
  ASSERT(Node != NULL);
  Node->Buffer = AllocateZeroPool(AsciiStrSize(Buffer) * sizeof (CHAR16));
  ASSERT(Node->Buffer != NULL);
  Ascii2UnicodeString ((CHAR8*)Buffer, Node->Buffer);

  InsertTailList(&gViewingSettings.CommandHistory.Link, &Node->Link);
}

/**
  Move the cursor position one character backward.

  @param[in] LineLength       Length of a line. Get it by calling QueryMode
  @param[in, out] Column      Current column of the cursor position
  @param[in, out] Row         Current row of the cursor position
**/
VOID
EFIAPI
MoveCursorBackward (
  IN     UINTN                   LineLength,
  IN OUT UINTN                   *Column,
  IN OUT UINTN                   *Row
  )
{
  //
  // If current column is 0, move to the last column of the previous line,
  // otherwise, just decrement column.
  //
  if (*Column == 0) {
    *Column = LineLength - 1;
    if (*Row > 0) {
      (*Row)--;
    }
    return;
  }
  (*Column)--;
}



/**
  Move the cursor position one character forward.

  @param[in] LineLength       Length of a line.
  @param[in] TotalRow         Total row of a screen
  @param[in, out] Column      Current column of the cursor position
  @param[in, out] Row         Current row of the cursor position
**/
VOID
EFIAPI
MoveCursorForward (
  IN     UINTN                   LineLength,
  IN     UINTN                   TotalRow,
  IN OUT UINTN                   *Column,
  IN OUT UINTN                   *Row
  )
{
  //
  // Increment Column.
  // If this puts column past the end of the line, move to first column
  // of the next row.
  //
  (*Column)++;
  if (*Column >= LineLength) {
    (*Column) = 0;
    if ((*Row) < TotalRow - 1) {
      (*Row)++;
    }
  }
}



/**
  File style interface for console (Read).

  This will return a single line of input from the console.

  @param This           A pointer to the EFI_FILE_PROTOCOL instance that is the
                        file handle to read data from. Not used.
  @param BufferSize     On input, the size of the Buffer. On output, the amount
                        of data returned in Buffer. In both cases, the size is
                        measured in bytes.
  @param Buffer         The buffer into which the data is read.


  @retval EFI_SUCCESS           The data was read.
  @retval EFI_NO_MEDIA          The device has no medium.
  @retval EFI_DEVICE_ERROR      The device reported an error.
  @retval EFI_DEVICE_ERROR      An attempt was made to read from a deleted file.
  @retval EFI_DEVICE_ERROR      On entry, the current file position is beyond the end of the file.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current directory
                                entry. BufferSize has been updated with the size
                                needed to complete the request.
  @retval EFI_OUT_OF_RESOURCES  A memory allocation failed.
**/
EFI_STATUS
EFIAPI
_readline (
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
{
  CHAR16              *CurrentString;
  BOOLEAN             Done;
  UINTN               Column;         // Column of current cursor
  UINTN               Row;            // Row of current cursor
  UINTN               StartColumn;    // Column at the beginning of the line
  UINTN               Update;         // Line index for update
  UINTN               Delete;         // Num of chars to delete from console after update
  UINTN               StringLen;      // Total length of the line
  UINTN               StringCurPos;   // Line index corresponding to the cursor
  UINTN               MaxStr;         // Maximum possible line length
//UINTN               Index;
  UINTN               TotalColumn;     // Num of columns in the console
  UINTN               TotalRow;       // Num of rows in the console
  UINTN               SkipLength;
  UINTN               OutputLength;   // Length of the update string
  UINTN               TailRow;        // Row of end of line
  UINTN               TailColumn;     // Column of end of line
  EFI_INPUT_KEY       Key;

  BUFFER_LIST         *LinePos;
  BUFFER_LIST         *NewPos;
  BOOLEAN             InScrolling;
  EFI_STATUS          Status;
  BOOLEAN             InTabScrolling; // Whether in TAB-completion state
//EFI_SHELL_FILE_INFO *FoundFileList;
//EFI_SHELL_FILE_INFO *TabLinePos;
//EFI_SHELL_FILE_INFO *TempPos;
  CHAR16              *TabStr;
  CHAR16              *TabOutputStr;
//BOOLEAN             InQuotationMode;
//CHAR16              *TempStr;
  UINTN               TabPos;         // Start index of the string to search for TAB completion.
  UINTN               TabUpdatePos;   // Start index of the string updated by TAB stroke
//  UINTN               Count;
  UINTN               EventIndex;
//CONST CHAR16        *Cwd;

  //
  // If buffer is not large enough to hold a CHAR16, return minimum buffer size
  //
  if (*BufferSize < sizeof (CHAR16) * 2) {
    *BufferSize = sizeof (CHAR16) * 2;
    return (EFI_BUFFER_TOO_SMALL);
  }

  Done              = FALSE;
  CurrentString     = Buffer;
  StringLen         = 0;
  StringCurPos      = 0;
  OutputLength      = 0;
  Update            = 0;
  Delete            = 0;
  LinePos           = NewPos = (BUFFER_LIST*)(&gViewingSettings.CommandHistory);
  InScrolling       = FALSE;
  InTabScrolling    = FALSE;
  Status            = EFI_SUCCESS;
//TabLinePos        = NULL;
//FoundFileList     = NULL;
//TempPos           = NULL;
  TabPos            = 0;
  TabUpdatePos      = 0;

  //
  // Allocate buffers
  //
  TabStr            = AllocateZeroPool (*BufferSize);
  if (TabStr == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  TabOutputStr      = AllocateZeroPool (*BufferSize);
  if (TabOutputStr == NULL) {
    FreePool(TabStr);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Get the screen setting and the current cursor location
  //
  Column      = StartColumn = gST->ConOut->Mode->CursorColumn;
  Row         = gST->ConOut->Mode->CursorRow;
  gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &TotalColumn, &TotalRow);

  //
  // Limit the line length to the buffer size or the minimun size of the
  // screen. (The smaller takes effect)
  //
  MaxStr = TotalColumn * (TotalRow - 1) - StartColumn;
  if (MaxStr > *BufferSize / sizeof (CHAR16)) {
    MaxStr = *BufferSize / sizeof (CHAR16);
  }
  ZeroMem (CurrentString, MaxStr * sizeof (CHAR16));
  do {
    //
    // Read a key
    //
    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &EventIndex);
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Press PageUp or PageDown to scroll the history screen up or down.
    // Press any other key to quit scrolling.
    //
    if (Key.UnicodeChar == 0 && (Key.ScanCode == SCAN_PAGE_UP || Key.ScanCode == SCAN_PAGE_DOWN)) {
//    if (Key.ScanCode == SCAN_PAGE_UP) {
//      ConsoleLoggerDisplayHistory(FALSE, 0, ShellInfoObject.ConsoleInfo);
//    } else if (Key.ScanCode == SCAN_PAGE_DOWN) {
//      ConsoleLoggerDisplayHistory(TRUE, 0, ShellInfoObject.ConsoleInfo);
//    }

      InScrolling = TRUE;
    } else {
      if (InScrolling) {
//      ConsoleLoggerStopHistory(ShellInfoObject.ConsoleInfo);
//      InScrolling = FALSE;
      }
    }

    //
    // If we are quitting TAB scrolling...
    //
    if (InTabScrolling && Key.UnicodeChar != CHAR_TAB) {
//      if (FoundFileList != NULL) {
//        ShellInfoObject.NewEfiShellProtocol->FreeFileList (&FoundFileList);
//        DEBUG_CODE(FoundFileList = NULL;);
//      }
//      InTabScrolling = FALSE;
    }

    switch (Key.UnicodeChar) {
    case CHAR_CARRIAGE_RETURN:
      //
      // All done, print a newline at the end of the string
      //
      TailRow     = Row + (StringLen - StringCurPos + Column) / TotalColumn;
      TailColumn  = (StringLen - StringCurPos + Column) % TotalColumn;
      if (TailColumn != -1 && TailRow != -1) {
        Status = gST->ConOut->SetCursorPosition(gST->ConOut, TailColumn, TailRow);
      }
      Print (L"\n");
//    ShellPrintEx ((INT32)TailColumn, (INT32)TailRow, L"%N\n");
      Done = TRUE;
      break;

    case CHAR_BACKSPACE:
      if (StringCurPos != 0) {
        //
        // If not move back beyond string beginning, move all characters behind
        // the current position one character forward
        //
        StringCurPos--;
        Update  = StringCurPos;
        Delete  = 1;
        CopyMem (CurrentString + StringCurPos, CurrentString + StringCurPos + 1, sizeof (CHAR16) * (StringLen - StringCurPos));

        //
        // Adjust the current column and row
        //
        MoveCursorBackward (TotalColumn, &Column, &Row);
      }
      break;

    case CHAR_TAB:
      //
      // handle auto complete of file and directory names...
      //
//    if (InTabScrolling) {
//      ASSERT(FoundFileList != NULL);
//      ASSERT(TabLinePos != NULL);
//      TabLinePos = (EFI_SHELL_FILE_INFO*)GetNextNode(&(FoundFileList->Link), &TabLinePos->Link);
//      if (IsNull(&(FoundFileList->Link), &TabLinePos->Link)) {
//        TabLinePos = (EFI_SHELL_FILE_INFO*)GetNextNode(&(FoundFileList->Link), &TabLinePos->Link);
//      }
//    } else {
//      TabPos          = 0;
//      TabUpdatePos    = 0;
//      InQuotationMode = FALSE;
//      for (Index = 0; Index < StringLen; Index++) {
//        if (CurrentString[Index] == L'\"') {
//          InQuotationMode = (BOOLEAN)(!InQuotationMode);
//        }
//        if (CurrentString[Index] == L' ' && !InQuotationMode) {
//          TabPos = Index + 1;
//          TabUpdatePos = Index + 1;
//        }
//        if (CurrentString[Index] == L'\\') {
//          TabUpdatePos = Index + 1;
//        }
//      }
//      if (StrStr(CurrentString + TabPos, L":") == NULL) {
//        Cwd = ShellInfoObject.NewEfiShellProtocol->GetCurDir(NULL);
//        if (Cwd != NULL) {
//          StrCpy(TabStr, Cwd);
//          if (TabStr[StrLen(TabStr)-1] == L'\\' && *(CurrentString + TabPos) == L'\\' ) {
//            TabStr[StrLen(TabStr)-1] = CHAR_NULL;
//          }
//          StrnCat(TabStr, CurrentString + TabPos, (StringLen - TabPos) * sizeof (CHAR16));
//        } else {
//          StrCpy(TabStr, L"");
//          StrnCat(TabStr, CurrentString + TabPos, (StringLen - TabPos) * sizeof (CHAR16));
//        }
//      } else {
//        StrCpy(TabStr, CurrentString + TabPos);
//      }
//      StrCat(TabStr, L"*");
//      FoundFileList = NULL;
////        TabStr = PathCleanUpDirectories(TabStr);
//      Status  = ShellInfoObject.NewEfiShellProtocol->FindFiles(TabStr, &FoundFileList);
//      for ( TempStr = CurrentString
//          ; *TempStr == L' '
//          ; TempStr++); // note the ';'... empty for loop
//      //
//      // make sure we have a list before we do anything more...
//      //
//      if (EFI_ERROR (Status) || FoundFileList == NULL) {
//        InTabScrolling = FALSE;
//        TabLinePos = NULL;
//        continue;
//      } else {
//        //
//        // enumerate through the list of files
//        //
//        for ( TempPos = (EFI_SHELL_FILE_INFO*)GetFirstNode(&(FoundFileList->Link))
//            ; !IsNull(&FoundFileList->Link, &TempPos->Link)
//            ; TempPos = (EFI_SHELL_FILE_INFO*)GetNextNode(&(FoundFileList->Link), &(TempPos->Link))
//           ){
//          //
//          // If "cd" is typed, only directory name will be auto-complete filled
//          // in either case . and .. will be removed.
//          //
//          if ((((TempStr[0] == L'c' || TempStr[0] == L'C') &&
//              (TempStr[1] == L'd' || TempStr[1] == L'D')
//             ) && ((ShellIsDirectory(TempPos->FullName) != EFI_SUCCESS)
//              ||(StrCmp(TempPos->FileName, L".") == 0)
//              ||(StrCmp(TempPos->FileName, L"..") == 0)
//             )) || ((StrCmp(TempPos->FileName, L".") == 0)
//              ||(StrCmp(TempPos->FileName, L"..") == 0))){
//              TabLinePos = TempPos;
//              TempPos = (EFI_SHELL_FILE_INFO*)(RemoveEntryList(&(TempPos->Link))->BackLink);
//              InternalFreeShellFileInfoNode(TabLinePos);
//          }
//        }
//        if (FoundFileList != NULL && !IsListEmpty(&FoundFileList->Link)) {
//          TabLinePos = (EFI_SHELL_FILE_INFO*)GetFirstNode(&FoundFileList->Link);
//          InTabScrolling = TRUE;
//        } else {
//          FreePool(FoundFileList);
//          FoundFileList = NULL;
//        }
//      }
//    }
      break;

    default:
      if (Key.UnicodeChar >= ' ') {
        //
        // If we are at the buffer's end, drop the key
        //
        if (StringLen == MaxStr - 1 && (gViewingSettings.InsertMode || StringCurPos == StringLen)) {
          break;
        }
        //
        // If in insert mode, make space by moving each other character 1
        // space higher in the array
        //
        if (gViewingSettings.InsertMode) {
          CopyMem(CurrentString + StringCurPos + 1, CurrentString + StringCurPos, (StringLen - StringCurPos)*sizeof(CurrentString[0]));
        }

        CurrentString[StringCurPos] = Key.UnicodeChar;
        Update      = StringCurPos;

        StringCurPos += 1;
        OutputLength = 1;
      }
      break;

    case 0:
      switch (Key.ScanCode) {
      case SCAN_DELETE:
        //
        // Move characters behind current position one character forward
        //
        if (StringLen != 0) {
          Update  = StringCurPos;
          Delete  = 1;
          CopyMem (CurrentString + StringCurPos, CurrentString + StringCurPos + 1, sizeof (CHAR16) * (StringLen - StringCurPos));
        }
        break;

      case SCAN_UP:
        //
        // Prepare to print the previous command
        //
        NewPos = (BUFFER_LIST*)GetPreviousNode(&gViewingSettings.CommandHistory.Link, &LinePos->Link);
        if (IsNull(&gViewingSettings.CommandHistory.Link, &LinePos->Link)) {
          NewPos = (BUFFER_LIST*)GetPreviousNode(&gViewingSettings.CommandHistory.Link, &LinePos->Link);
        }
        break;

      case SCAN_DOWN:
        //
        // Prepare to print the next command
        //
        NewPos = (BUFFER_LIST*)GetNextNode(&gViewingSettings.CommandHistory.Link, &LinePos->Link);
        if (NewPos == (BUFFER_LIST*)(&gViewingSettings.CommandHistory)) {
          NewPos = (BUFFER_LIST*)GetNextNode(&gViewingSettings.CommandHistory.Link, &LinePos->Link);
        }
        break;

      case SCAN_LEFT:
        //
        // Adjust current cursor position
        //
        if (StringCurPos != 0) {
          --StringCurPos;
          MoveCursorBackward (TotalColumn, &Column, &Row);
        }
        break;

      case SCAN_RIGHT:
        //
        // Adjust current cursor position
        //
        if (StringCurPos < StringLen) {
          ++StringCurPos;
          MoveCursorForward (TotalColumn, TotalRow, &Column, &Row);
        }
        break;

      case SCAN_HOME:
        //
        // Move current cursor position to the beginning of the command line
        //
        Row -= (StringCurPos + StartColumn) / TotalColumn;
        Column  = StartColumn;
        StringCurPos  = 0;
        break;

      case SCAN_END:
        //
        // Move current cursor position to the end of the command line
        //
        TailRow       = Row + (StringLen - StringCurPos + Column) / TotalColumn;
        TailColumn    = (StringLen - StringCurPos + Column) % TotalColumn;
        Row           = TailRow;
        Column        = TailColumn;
        StringCurPos  = StringLen;
        break;

      case SCAN_ESC:
        //
        // Prepare to clear the current command line
        //
        CurrentString[0]  = 0;
        Update  = 0;
        Delete  = StringLen;
        Row -= (StringCurPos + StartColumn) / TotalColumn;
        Column        = StartColumn;
        OutputLength  = 0;
        break;

      case SCAN_INSERT:
        //
        // Toggle the SEnvInsertMode flag
        //
        gViewingSettings.InsertMode = (BOOLEAN)!gViewingSettings.InsertMode;
        break;

      case SCAN_F7:
        //
        // Print command history
        //
//      PrintCommandHistory (TotalColumn, TotalRow, 4);
        *CurrentString  = CHAR_NULL;
        Done  = TRUE;
        break;
      }
    }

    if (Done) {
      break;
    }

    //
    // If we are in auto-complete mode, we are preparing to print
    // the next file or directory name
    //
    if (InTabScrolling) {
//    //
//    // Adjust the column and row to the start of TAB-completion string.
//    //
//    Column = (StartColumn + TabUpdatePos) % TotalColumn;
//    Row -= (StartColumn + StringCurPos) / TotalColumn - (StartColumn + TabUpdatePos) / TotalColumn;
//    OutputLength = StrLen (TabLinePos->FileName);
//    //
//    // if the output string contains  blank space, quotation marks L'\"'
//    // should be added to the output.
//    //
//    if (StrStr(TabLinePos->FileName, L" ") != NULL){
//      TabOutputStr[0] = L'\"';
//      CopyMem (TabOutputStr + 1, TabLinePos->FileName, OutputLength * sizeof (CHAR16));
//      TabOutputStr[OutputLength + 1] = L'\"';
//      TabOutputStr[OutputLength + 2] = CHAR_NULL;
//    } else {
//      CopyMem (TabOutputStr, TabLinePos->FileName, OutputLength * sizeof (CHAR16));
//      TabOutputStr[OutputLength] = CHAR_NULL;
//    }
//    OutputLength = StrLen (TabOutputStr) < MaxStr - 1 ? StrLen (TabOutputStr) : MaxStr - 1;
//    CopyMem (CurrentString + TabUpdatePos, TabOutputStr, OutputLength * sizeof (CHAR16));
//    CurrentString[TabUpdatePos + OutputLength] = CHAR_NULL;
//    StringCurPos = TabUpdatePos + OutputLength;
//    Update = TabUpdatePos;
//    if (StringLen > TabUpdatePos + OutputLength) {
//      Delete = StringLen - TabUpdatePos - OutputLength;
//    }
    }

    //
    // If we have a new position, we are preparing to print a previous or
    // next command.
    //
    if (NewPos != (BUFFER_LIST*)(&gViewingSettings.CommandHistory)) {
      Column = StartColumn;
      Row -= (StringCurPos + StartColumn) / TotalColumn;

      LinePos       = NewPos;
      NewPos        = (BUFFER_LIST*)(&gViewingSettings.CommandHistory);

      OutputLength  = StrLen (LinePos->Buffer) < MaxStr - 1 ? StrLen (LinePos->Buffer) : MaxStr - 1;
      CopyMem (CurrentString, LinePos->Buffer, OutputLength * sizeof (CHAR16));
      CurrentString[OutputLength] = CHAR_NULL;

      StringCurPos            = OutputLength;

      //
      // Draw new input string
      //
      Update = 0;
      if (StringLen > OutputLength) {
        //
        // If old string was longer, blank its tail
        //
        Delete = StringLen - OutputLength;
      }
    }
    //
    // If we need to update the output do so now
    //
    if (Update != (UINTN) -1) {
      if (Column != -1 && Row != -1) {
        Status = gST->ConOut->SetCursorPosition(gST->ConOut, Column, Row);
      }
      Print (L"%s%.*s", CurrentString + Update, Delete, L"");
//    ShellPrintEx ((INT32)Column, (INT32)Row, L"%s%.*s", CurrentString + Update, Delete, L"");

      StringLen = StrLen (CurrentString);

      if (Delete != 0) {
        SetMem (CurrentString + StringLen, Delete * sizeof (CHAR16), CHAR_NULL);
      }

      if (StringCurPos > StringLen) {
        StringCurPos = StringLen;
      }

      Update = (UINTN) -1;

      //
      // After using print to reflect newly updates, if we're not using
      // BACKSPACE and DELETE, we need to move the cursor position forward,
      // so adjust row and column here.
      //
      if (Key.UnicodeChar != CHAR_BACKSPACE && !(Key.UnicodeChar == 0 && Key.ScanCode == SCAN_DELETE)) {
        //
        // Calulate row and column of the tail of current string
        //
        TailRow     = Row + (StringLen - StringCurPos + Column + OutputLength) / TotalColumn;
        TailColumn  = (StringLen - StringCurPos + Column + OutputLength) % TotalColumn;

        //
        // If the tail of string reaches screen end, screen rolls up, so if
        // Row does not equal TailRow, Row should be decremented
        //
        // (if we are recalling commands using UPPER and DOWN key, and if the
        // old command is too long to fit the screen, TailColumn must be 79.
        //
        if (TailColumn == 0 && TailRow >= TotalRow && Row != TailRow) {
          Row--;
        }
        //
        // Calculate the cursor position after current operation. If cursor
        // reaches line end, update both row and column, otherwise, only
        // column will be changed.
        //
        if (Column + OutputLength >= TotalColumn) {
          SkipLength = OutputLength - (TotalColumn - Column);

          Row += SkipLength / TotalColumn + 1;
          if (Row > TotalRow - 1) {
            Row = TotalRow - 1;
          }

          Column = SkipLength % TotalColumn;
        } else {
          Column += OutputLength;
        }
      }

      Delete = 0;
    }
    //
    // Set the cursor position for this key
    //
    gST->ConOut->SetCursorPosition (gST->ConOut, Column, Row);
  } while (!Done);

  if (CurrentString != NULL && StrLen(CurrentString) > 0) {
    //
    // add the line to the history buffer
    //
//  AddLineToCommandHistory(CurrentString);
  }

  FreePool (TabStr);
  FreePool (TabOutputStr);
  //
  // Return the data to the caller
  //
  *BufferSize = StringLen * sizeof (CHAR16);

  //
  // if this was used it should be deallocated by now...
  // prevent memory leaks...
  //
//ASSERT(FoundFileList == NULL);

  return EFI_SUCCESS;
}

