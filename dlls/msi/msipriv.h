/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2005 Mike McCormack for CodeWeavers
 * Copyright 2005 Aric Stewart for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_MSI_PRIVATE__
#define __WINE_MSI_PRIVATE__

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "msi.h"
#include "msiquery.h"
#include "objbase.h"
#include "objidl.h"
#include "winnls.h"
#include "wine/list.h"

#define MSI_DATASIZEMASK 0x00ff
#define MSITYPE_VALID    0x0100
#define MSITYPE_LOCALIZABLE 0x200
#define MSITYPE_STRING   0x0800
#define MSITYPE_NULLABLE 0x1000
#define MSITYPE_KEY      0x2000

/* Word Count masks */
#define MSIWORDCOUNT_SHORTFILENAMES     0x0001
#define MSIWORDCOUNT_COMPRESSED         0x0002
#define MSIWORDCOUNT_ADMINISTRATIVE     0x0004
#define MSIWORDCOUNT_PRIVILEGES         0x0008

#define MSITYPE_IS_BINARY(type) (((type) & ~MSITYPE_NULLABLE) == (MSITYPE_STRING|MSITYPE_VALID))

struct tagMSITABLE;
typedef struct tagMSITABLE MSITABLE;

struct string_table;
typedef struct string_table string_table;

struct tagMSIOBJECTHDR;
typedef struct tagMSIOBJECTHDR MSIOBJECTHDR;

typedef VOID (*msihandledestructor)( MSIOBJECTHDR * );

struct tagMSIOBJECTHDR
{
    UINT magic;
    UINT type;
    LONG refcount;
    msihandledestructor destructor;
};

typedef struct tagMSIDATABASE
{
    MSIOBJECTHDR hdr;
    IStorage *storage;
    string_table *strings;
    LPWSTR path;
    LPWSTR deletefile;
    LPCWSTR mode;
    struct list tables;
    struct list transforms;
} MSIDATABASE;

typedef struct tagMSIVIEW MSIVIEW;

typedef struct tagMSIQUERY
{
    MSIOBJECTHDR hdr;
    MSIVIEW *view;
    UINT row;
    MSIDATABASE *db;
    struct list mem;
} MSIQUERY;

/* maybe we can use a Variant instead of doing it ourselves? */
typedef struct tagMSIFIELD
{
    UINT type;
    union
    {
        INT iVal;
        LPWSTR szwVal;
        IStream *stream;
    } u;
} MSIFIELD;

typedef struct tagMSIRECORD
{
    MSIOBJECTHDR hdr;
    UINT count;       /* as passed to MsiCreateRecord */
    MSIFIELD fields[1]; /* nb. array size is count+1 */
} MSIRECORD;

typedef void *MSIITERHANDLE;

typedef struct tagMSIVIEWOPS
{
    /*
     * fetch_int - reads one integer from {row,col} in the table
     *
     *  This function should be called after the execute method.
     *  Data returned by the function should not change until
     *   close or delete is called.
     *  To get a string value, query the database's string table with
     *   the integer value returned from this function.
     */
    UINT (*fetch_int)( struct tagMSIVIEW *, UINT row, UINT col, UINT *val );

    /*
     * fetch_stream - gets a stream from {row,col} in the table
     *
     *  This function is similar to fetch_int, except fetches a
     *    stream instead of an integer.
     */
    UINT (*fetch_stream)( struct tagMSIVIEW *, UINT row, UINT col, IStream **stm );

    /*
     * set_row - sets values in a row as specified by mask
     *
     *  Similar semantics to fetch_int
     */
    UINT (*set_row)( struct tagMSIVIEW *, UINT row, MSIRECORD *rec, UINT mask );

    /*
     * Inserts a new row into the database from the records contents
     */
    UINT (*insert_row)( struct tagMSIVIEW *, MSIRECORD * );

    /*
     * execute - loads the underlying data into memory so it can be read
     */
    UINT (*execute)( struct tagMSIVIEW *, MSIRECORD * );

    /*
     * close - clears the data read by execute from memory
     */
    UINT (*close)( struct tagMSIVIEW * );

    /*
     * get_dimensions - returns the number of rows or columns in a table.
     *
     *  The number of rows can only be queried after the execute method
     *   is called. The number of columns can be queried at any time.
     */
    UINT (*get_dimensions)( struct tagMSIVIEW *, UINT *rows, UINT *cols );

    /*
     * get_column_info - returns the name and type of a specific column
     *
     *  The name is HeapAlloc'ed by this function and should be freed by
     *   the caller.
     *  The column information can be queried at any time.
     */
    UINT (*get_column_info)( struct tagMSIVIEW *, UINT n, LPWSTR *name, UINT *type );

    /*
     * modify - not yet implemented properly
     */
    UINT (*modify)( struct tagMSIVIEW *, MSIMODIFY, MSIRECORD * );

    /*
     * delete - destroys the structure completely
     */
    UINT (*delete)( struct tagMSIVIEW * );

    /*
     * find_matching_rows - iterates through rows that match a value
     *
     * If the column type is a string then a string ID should be passed in.
     *  If the value to be looked up is an integer then no transformation of
     *  the input value is required, except if the column is a string, in which
     *  case a string ID should be passed in.
     * The handle is an input/output parameter that keeps track of the current
     *  position in the iteration. It must be initialised to zero before the
     *  first call and continued to be passed in to subsequent calls.
     */
    UINT (*find_matching_rows)( struct tagMSIVIEW *, UINT col, UINT val, UINT *row, MSIITERHANDLE *handle );
} MSIVIEWOPS;

struct tagMSIVIEW
{
    MSIOBJECTHDR hdr;
    const MSIVIEWOPS *ops;
};

struct msi_dialog_tag;
typedef struct msi_dialog_tag msi_dialog;

#define PROPERTY_HASH_SIZE 67

typedef struct tagMSIPACKAGE
{
    MSIOBJECTHDR hdr;
    MSIDATABASE *db;
    struct list components;
    struct list features;
    struct list files;
    struct list tempfiles;
    struct list folders;
    LPWSTR ActionFormat;
    LPWSTR LastAction;

    struct list classes;
    struct list extensions;
    struct list progids;
    struct list mimes;
    struct list appids;

    struct tagMSISCRIPT *script;

    struct list RunningActions;

    LPWSTR PackagePath;
    LPWSTR ProductCode;

    UINT CurrentInstallState;
    msi_dialog *dialog;
    LPWSTR next_dialog;
    float center_x;
    float center_y;

    UINT WordCount;

    struct list props[PROPERTY_HASH_SIZE];

    struct list subscriptions;
} MSIPACKAGE;

typedef struct tagMSIPREVIEW
{
    MSIOBJECTHDR hdr;
    MSIPACKAGE *package;
    msi_dialog *dialog;
} MSIPREVIEW;

#define MSI_MAX_PROPS 20

typedef struct tagMSISUMMARYINFO
{
    MSIOBJECTHDR hdr;
    IStorage *storage;
    DWORD update_count;
    PROPVARIANT property[MSI_MAX_PROPS];
} MSISUMMARYINFO;

typedef struct tagMSIFEATURE
{
    struct list entry;
    LPWSTR Feature;
    LPWSTR Feature_Parent;
    LPWSTR Title;
    LPWSTR Description;
    INT Display;
    INT Level;
    LPWSTR Directory;
    INT Attributes;
    INSTALLSTATE Installed;
    INSTALLSTATE ActionRequest;
    INSTALLSTATE Action;
    struct list Children;
    struct list Components;
    INT Cost;
} MSIFEATURE;

typedef struct tagMSICOMPONENT
{
    struct list entry;
    DWORD magic;
    LPWSTR Component;
    LPWSTR ComponentId;
    LPWSTR Directory;
    INT Attributes;
    LPWSTR Condition;
    LPWSTR KeyPath;
    INSTALLSTATE Installed;
    INSTALLSTATE ActionRequest;
    INSTALLSTATE Action;
    BOOL ForceLocalState;
    BOOL Enabled;
    INT  Cost;
    INT  RefCount;
    LPWSTR FullKeypath;
    LPWSTR AdvertiseString;
} MSICOMPONENT;

typedef struct tagComponentList
{
    struct list entry;
    MSICOMPONENT *component;
} ComponentList;

typedef struct tagFeatureList
{
    struct list entry;
    MSIFEATURE *feature;
} FeatureList;

typedef struct tagMSIFOLDER
{
    struct list entry;
    LPWSTR Directory;
    LPWSTR TargetDefault;
    LPWSTR SourceLongPath;
    LPWSTR SourceShortPath;

    LPWSTR ResolvedTarget;
    LPWSTR ResolvedSource;
    LPWSTR Property;   /* initially set property */
    struct tagMSIFOLDER *Parent;
    INT   State;
        /* 0 = uninitialized */
        /* 1 = existing */
        /* 2 = created remove if empty */
        /* 3 = created persist if empty */
    INT   Cost;
    INT   Space;
} MSIFOLDER;

typedef enum _msi_file_state {
    msifs_invalid,
    msifs_missing,
    msifs_overwrite,
    msifs_present,
    msifs_installed,
    msifs_skipped,
} msi_file_state;

typedef struct tagMSIFILE
{
    struct list entry;
    LPWSTR File;
    MSICOMPONENT *Component;
    LPWSTR FileName;
    LPWSTR ShortName;
    LPWSTR LongName;
    INT FileSize;
    LPWSTR Version;
    LPWSTR Language;
    INT Attributes;
    INT Sequence;
    msi_file_state state;
    LPWSTR  SourcePath;
    LPWSTR  TargetPath;
    BOOL IsCompressed;
} MSIFILE;

typedef struct tagMSITEMPFILE
{
    struct list entry;
    LPWSTR File;
    LPWSTR Path;
} MSITEMPFILE;

typedef struct tagMSIAPPID
{
    struct list entry;
    LPWSTR AppID; /* Primary key */
    LPWSTR RemoteServerName;
    LPWSTR LocalServer;
    LPWSTR ServiceParameters;
    LPWSTR DllSurrogate;
    BOOL ActivateAtStorage;
    BOOL RunAsInteractiveUser;
} MSIAPPID;

typedef struct tagMSIPROGID MSIPROGID;

typedef struct tagMSICLASS
{
    struct list entry;
    LPWSTR clsid;     /* Primary Key */
    LPWSTR Context;   /* Primary Key */
    MSICOMPONENT *Component;
    MSIPROGID *ProgID;
    LPWSTR ProgIDText;
    LPWSTR Description;
    MSIAPPID *AppID;
    LPWSTR FileTypeMask;
    LPWSTR IconPath;
    LPWSTR DefInprocHandler;
    LPWSTR DefInprocHandler32;
    LPWSTR Argument;
    MSIFEATURE *Feature;
    INT Attributes;
    /* not in the table, set during installation */
    BOOL Installed;
} MSICLASS;

typedef struct tagMSIMIME MSIMIME;

typedef struct tagMSIEXTENSION
{
    struct list entry;
    LPWSTR Extension;  /* Primary Key */
    MSICOMPONENT *Component;
    MSIPROGID *ProgID;
    LPWSTR ProgIDText;
    MSIMIME *Mime;
    MSIFEATURE *Feature;
    /* not in the table, set during installation */
    BOOL Installed;
    struct list verbs;
} MSIEXTENSION;

struct tagMSIPROGID
{
    struct list entry;
    LPWSTR ProgID;  /* Primary Key */
    MSIPROGID *Parent;
    MSICLASS *Class;
    LPWSTR Description;
    LPWSTR IconPath;
    /* not in the table, set during installation */
    BOOL InstallMe;
    MSIPROGID *CurVer;
    MSIPROGID *VersionInd;
};

typedef struct tagMSIVERB
{
    struct list entry;
    LPWSTR Verb;
    INT Sequence;
    LPWSTR Command;
    LPWSTR Argument;
} MSIVERB;

struct tagMSIMIME
{
    struct list entry;
    LPWSTR ContentType;  /* Primary Key */
    MSIEXTENSION *Extension;
    LPWSTR clsid;
    MSICLASS *Class;
    /* not in the table, set during installation */
    BOOL InstallMe;
};

enum SCRIPTS {
    INSTALL_SCRIPT = 0,
    COMMIT_SCRIPT = 1,
    ROLLBACK_SCRIPT = 2,
    TOTAL_SCRIPTS = 3
};

#define SEQUENCE_UI       0x1
#define SEQUENCE_EXEC     0x2
#define SEQUENCE_INSTALL  0x10

typedef struct tagMSISCRIPT
{
    LPWSTR  *Actions[TOTAL_SCRIPTS];
    UINT    ActionCount[TOTAL_SCRIPTS];
    BOOL    ExecuteSequenceRun;
    BOOL    CurrentlyScripting;
    UINT    InWhatSequence;
    LPWSTR  *UniqueActions;
    UINT    UniqueActionsCount;
} MSISCRIPT;

#define MSIHANDLETYPE_ANY 0
#define MSIHANDLETYPE_DATABASE 1
#define MSIHANDLETYPE_SUMMARYINFO 2
#define MSIHANDLETYPE_VIEW 3
#define MSIHANDLETYPE_RECORD 4
#define MSIHANDLETYPE_PACKAGE 5
#define MSIHANDLETYPE_PREVIEW 6

#define MSI_MAJORVERSION 3
#define MSI_MINORVERSION 1
#define MSI_BUILDNUMBER 4000

#define GUID_SIZE 39

#define MSIHANDLE_MAGIC 0x4d434923

DEFINE_GUID(CLSID_IMsiServer,   0x000C101C,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(CLSID_IMsiServerX1, 0x000C103E,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(CLSID_IMsiServerX2, 0x000C1090,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(CLSID_IMsiServerX3, 0x000C1094,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);

DEFINE_GUID(CLSID_IMsiServerMessage, 0x000C101D,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);

/* handle unicode/ascii output in the Msi* API functions */
typedef struct {
    BOOL unicode;
    union {
       LPSTR a;
       LPWSTR w;
    } str;
} awstring;

typedef struct {
    BOOL unicode;
    union {
       LPCSTR a;
       LPCWSTR w;
    } str;
} awcstring;

UINT msi_strcpy_to_awstring( LPCWSTR str, awstring *awbuf, DWORD *sz );

/* handle functions */
extern void *msihandle2msiinfo(MSIHANDLE handle, UINT type);
extern MSIHANDLE alloc_msihandle( MSIOBJECTHDR * );
extern void *alloc_msiobject(UINT type, UINT size, msihandledestructor destroy );
extern void msiobj_addref(MSIOBJECTHDR *);
extern int msiobj_release(MSIOBJECTHDR *);
extern void msiobj_lock(MSIOBJECTHDR *);
extern void msiobj_unlock(MSIOBJECTHDR *);
extern void msi_free_handle_table(void);

extern void free_cached_tables( MSIDATABASE *db );
extern void msi_free_transforms( MSIDATABASE *db );
extern string_table *load_string_table( IStorage *stg );
extern UINT MSI_CommitTables( MSIDATABASE *db );
extern HRESULT init_string_table( IStorage *stg );


/* string table functions */
extern BOOL msi_addstring( string_table *st, UINT string_no, const CHAR *data, int len, UINT refcount );
extern BOOL msi_addstringW( string_table *st, UINT string_no, const WCHAR *data, int len, UINT refcount );
extern UINT msi_id2stringW( string_table *st, UINT string_no, LPWSTR buffer, UINT *sz );
extern UINT msi_id2stringA( string_table *st, UINT string_no, LPSTR buffer, UINT *sz );

extern UINT msi_string2idW( string_table *st, LPCWSTR buffer, UINT *id );
extern UINT msi_string2idA( string_table *st, LPCSTR str, UINT *id );
extern string_table *msi_init_stringtable( int entries, UINT codepage );
extern VOID msi_destroy_stringtable( string_table *st );
extern UINT msi_string_count( string_table *st );
extern UINT msi_id_refcount( string_table *st, UINT i );
extern UINT msi_string_totalsize( string_table *st, UINT *datasize, UINT *poolsize );
extern UINT msi_strcmp( string_table *st, UINT lval, UINT rval, UINT *res );
extern const WCHAR *msi_string_lookup_id( string_table *st, UINT id );
extern UINT msi_string_get_codepage( string_table *st );


extern BOOL TABLE_Exists( MSIDATABASE *db, LPWSTR name );
extern MSICONDITION MSI_DatabaseIsTablePersistent( MSIDATABASE *db, LPCWSTR table );

extern UINT read_raw_stream_data( MSIDATABASE*, LPCWSTR stname,
                              USHORT **pdata, UINT *psz );

/* transform functions */
extern UINT msi_table_apply_transform( MSIDATABASE *db, IStorage *stg );
extern UINT MSI_DatabaseApplyTransformW( MSIDATABASE *db,
                 LPCWSTR szTransformFile, int iErrorCond );
extern void append_storage_to_db( MSIDATABASE *db, IStorage *stg );

/* action internals */
extern UINT MSI_InstallPackage( MSIPACKAGE *, LPCWSTR, LPCWSTR );
extern void ACTION_free_package_structures( MSIPACKAGE* );
extern UINT ACTION_DialogBox( MSIPACKAGE*, LPCWSTR);
extern UINT MSI_Sequence( MSIPACKAGE *package, LPCWSTR szTable, INT iSequenceMode );
extern UINT MSI_SetFeatureStates( MSIPACKAGE *package );

/* record internals */
extern UINT MSI_RecordSetIStream( MSIRECORD *, unsigned int, IStream *);
extern UINT MSI_RecordGetIStream( MSIRECORD *, unsigned int, IStream **);
extern const WCHAR *MSI_RecordGetString( MSIRECORD *, unsigned int );
extern MSIRECORD *MSI_CreateRecord( unsigned int );
extern UINT MSI_RecordSetInteger( MSIRECORD *, unsigned int, int );
extern UINT MSI_RecordSetStringW( MSIRECORD *, unsigned int, LPCWSTR );
extern UINT MSI_RecordSetStringA( MSIRECORD *, unsigned int, LPCSTR );
extern BOOL MSI_RecordIsNull( MSIRECORD *, unsigned int );
extern UINT MSI_RecordGetStringW( MSIRECORD * , unsigned int, LPWSTR, DWORD *);
extern UINT MSI_RecordGetStringA( MSIRECORD *, unsigned int, LPSTR, DWORD *);
extern int MSI_RecordGetInteger( MSIRECORD *, unsigned int );
extern UINT MSI_RecordReadStream( MSIRECORD *, unsigned int, char *, DWORD *);
extern unsigned int MSI_RecordGetFieldCount( MSIRECORD *rec );
extern UINT MSI_RecordSetStreamW( MSIRECORD *, unsigned int, LPCWSTR );
extern UINT MSI_RecordSetStreamA( MSIRECORD *, unsigned int, LPCSTR );
extern UINT MSI_RecordDataSize( MSIRECORD *, unsigned int );
extern UINT MSI_RecordStreamToFile( MSIRECORD *, unsigned int, LPCWSTR );
extern UINT MSI_RecordCopyField( MSIRECORD *, unsigned int, MSIRECORD *, unsigned int );

/* stream internals */
extern UINT get_raw_stream( MSIHANDLE hdb, LPCWSTR stname, IStream **stm );
extern UINT db_get_raw_stream( MSIDATABASE *db, LPCWSTR stname, IStream **stm );
extern void enum_stream_names( IStorage *stg );

/* database internals */
extern UINT MSI_OpenDatabaseW( LPCWSTR, LPCWSTR, MSIDATABASE ** );
extern UINT MSI_DatabaseOpenViewW(MSIDATABASE *, LPCWSTR, MSIQUERY ** );
extern UINT MSI_OpenQuery( MSIDATABASE *, MSIQUERY **, LPCWSTR, ... );
typedef UINT (*record_func)( MSIRECORD *, LPVOID );
extern UINT MSI_IterateRecords( MSIQUERY *, DWORD *, record_func, LPVOID );
extern MSIRECORD *MSI_QueryGetRecord( MSIDATABASE *db, LPCWSTR query, ... );
extern UINT MSI_DatabaseImport( MSIDATABASE *, LPCWSTR, LPCWSTR );
extern UINT MSI_DatabaseExport( MSIDATABASE *, LPCWSTR, LPCWSTR, LPCWSTR );
extern UINT MSI_DatabaseGetPrimaryKeys( MSIDATABASE *, LPCWSTR, MSIRECORD ** );

/* view internals */
extern UINT MSI_ViewExecute( MSIQUERY*, MSIRECORD * );
extern UINT MSI_ViewFetch( MSIQUERY*, MSIRECORD ** );
extern UINT MSI_ViewClose( MSIQUERY* );
extern UINT MSI_ViewGetColumnInfo(MSIQUERY *, MSICOLINFO, MSIRECORD **);
extern UINT VIEW_find_column( MSIVIEW *, LPCWSTR, UINT * );


/* install internals */
extern UINT MSI_SetInstallLevel( MSIPACKAGE *package, int iInstallLevel );

/* package internals */
extern MSIPACKAGE *MSI_CreatePackage( MSIDATABASE * );
extern UINT MSI_OpenPackageW( LPCWSTR szPackage, MSIPACKAGE ** );
extern UINT MSI_SetTargetPathW( MSIPACKAGE *, LPCWSTR, LPCWSTR );
extern UINT MSI_SetPropertyW( MSIPACKAGE *, LPCWSTR, LPCWSTR );
extern INT MSI_ProcessMessage( MSIPACKAGE *, INSTALLMESSAGE, MSIRECORD * );
extern UINT MSI_GetPropertyW( MSIPACKAGE *, LPCWSTR, LPWSTR, DWORD * );
extern UINT MSI_GetPropertyA(MSIPACKAGE *, LPCSTR, LPSTR, DWORD* );
extern MSICONDITION MSI_EvaluateConditionW( MSIPACKAGE *, LPCWSTR );
extern UINT MSI_GetComponentStateW( MSIPACKAGE *, LPCWSTR, INSTALLSTATE *, INSTALLSTATE * );
extern UINT MSI_GetFeatureStateW( MSIPACKAGE *, LPCWSTR, INSTALLSTATE *, INSTALLSTATE * );
extern UINT WINAPI MSI_SetFeatureStateW(MSIPACKAGE*, LPCWSTR, INSTALLSTATE );
extern LPCWSTR msi_download_file( LPCWSTR szUrl, LPWSTR filename );

/* for deformating */
extern UINT MSI_FormatRecordW( MSIPACKAGE *, MSIRECORD *, LPWSTR, DWORD * );
extern UINT MSI_FormatRecordA( MSIPACKAGE *, MSIRECORD *, LPSTR, DWORD * );

/* registry data encoding/decoding functions */
extern BOOL unsquash_guid(LPCWSTR in, LPWSTR out);
extern BOOL squash_guid(LPCWSTR in, LPWSTR out);
extern BOOL encode_base85_guid(GUID *,LPWSTR);
extern BOOL decode_base85_guid(LPCWSTR,GUID*);
extern UINT MSIREG_OpenUninstallKey(LPCWSTR szProduct, HKEY* key, BOOL create);
extern UINT MSIREG_OpenUserProductsKey(LPCWSTR szProduct, HKEY* key, BOOL create);
extern UINT MSIREG_OpenFeatures(HKEY* key);
extern UINT MSIREG_OpenFeaturesKey(LPCWSTR szProduct, HKEY* key, BOOL create);
extern UINT MSIREG_OpenComponents(HKEY* key);
extern UINT MSIREG_OpenUserComponentsKey(LPCWSTR szComponent, HKEY* key, BOOL create);
extern UINT MSIREG_OpenComponentsKey(LPCWSTR szComponent, HKEY* key, BOOL create);
extern UINT MSIREG_OpenProductsKey(LPCWSTR szProduct, HKEY* key, BOOL create);
extern UINT MSIREG_OpenUserFeaturesKey(LPCWSTR szProduct, HKEY* key, BOOL create);
extern UINT MSIREG_OpenUserComponentsKey(LPCWSTR szComponent, HKEY* key, BOOL create);
extern UINT MSIREG_OpenUpgradeCodesKey(LPCWSTR szProduct, HKEY* key, BOOL create);
extern UINT MSIREG_OpenUserUpgradeCodesKey(LPCWSTR szProduct, HKEY* key, BOOL create);

extern LPWSTR msi_reg_get_val_str( HKEY hkey, LPCWSTR name );
extern BOOL msi_reg_get_val_dword( HKEY hkey, LPCWSTR name, DWORD *val);

extern DWORD msi_version_str_to_dword(LPCWSTR p);
extern LPWSTR msi_version_dword_to_str(DWORD version);

extern LONG msi_reg_set_val_str( HKEY hkey, LPCWSTR name, LPCWSTR value );
extern LONG msi_reg_set_val_multi_str( HKEY hkey, LPCWSTR name, LPCWSTR value );
extern LONG msi_reg_set_val_dword( HKEY hkey, LPCWSTR name, DWORD val );
extern LONG msi_reg_set_subkey_val( HKEY hkey, LPCWSTR path, LPCWSTR name, LPCWSTR val );

/* msi dialog interface */
typedef UINT (*msi_dialog_event_handler)( MSIPACKAGE*, LPCWSTR, LPCWSTR, msi_dialog* );
extern msi_dialog *msi_dialog_create( MSIPACKAGE*, LPCWSTR, msi_dialog*, msi_dialog_event_handler );
extern UINT msi_dialog_run_message_loop( msi_dialog* );
extern void msi_dialog_end_dialog( msi_dialog* );
extern void msi_dialog_check_messages( HANDLE );
extern void msi_dialog_do_preview( msi_dialog* );
extern void msi_dialog_destroy( msi_dialog* );
extern BOOL msi_dialog_register_class( void );
extern void msi_dialog_unregister_class( void );
extern void msi_dialog_handle_event( msi_dialog*, LPCWSTR, LPCWSTR, MSIRECORD * );
extern UINT msi_dialog_reset( msi_dialog *dialog );
extern UINT msi_dialog_directorylist_up( msi_dialog *dialog );
extern msi_dialog *msi_dialog_get_parent( msi_dialog *dialog );
extern LPWSTR msi_dialog_get_name( msi_dialog *dialog );
extern UINT msi_spawn_error_dialog( MSIPACKAGE*, LPWSTR, LPWSTR );

/* preview */
extern MSIPREVIEW *MSI_EnableUIPreview( MSIDATABASE * );
extern UINT MSI_PreviewDialogW( MSIPREVIEW *, LPCWSTR );

/* summary information */
extern MSISUMMARYINFO *MSI_GetSummaryInformationW( IStorage *stg, UINT uiUpdateCount );
extern LPWSTR msi_suminfo_dup_string( MSISUMMARYINFO *si, UINT uiProperty );
extern LPWSTR msi_get_suminfo_product( IStorage *stg );

/* undocumented functions */
UINT WINAPI MsiCreateAndVerifyInstallerDirectory( DWORD );
UINT WINAPI MsiDecomposeDescriptorW( LPCWSTR, LPWSTR, LPWSTR, LPWSTR, DWORD * );
UINT WINAPI MsiDecomposeDescriptorA( LPCSTR, LPSTR, LPSTR, LPSTR, DWORD * );
LANGID WINAPI MsiLoadStringW( MSIHANDLE, UINT, LPWSTR, int, LANGID );
LANGID WINAPI MsiLoadStringA( MSIHANDLE, UINT, LPSTR, int, LANGID );

/* UI globals */
extern INSTALLUILEVEL gUILevel;
extern HWND gUIhwnd;
extern INSTALLUI_HANDLERA gUIHandlerA;
extern INSTALLUI_HANDLERW gUIHandlerW;
extern DWORD gUIFilter;
extern LPVOID gUIContext;
extern WCHAR gszLogFile[MAX_PATH];
extern HINSTANCE msi_hInstance;

/* action related functions */
extern UINT ACTION_PerformAction(MSIPACKAGE *package, const WCHAR *action, BOOL force);
extern UINT ACTION_PerformUIAction(MSIPACKAGE *package, const WCHAR *action);
extern void ACTION_FinishCustomActions( MSIPACKAGE* package);
extern UINT ACTION_CustomAction(MSIPACKAGE *package,const WCHAR *action, BOOL execute);

static inline void msi_feature_set_state( MSIFEATURE *feature, INSTALLSTATE state )
{
    feature->ActionRequest = state;
    feature->Action = state;
}

static inline void msi_component_set_state( MSICOMPONENT *comp, INSTALLSTATE state )
{
    comp->ActionRequest = state;
    comp->Action = state;
}

/* actions in other modules */
extern UINT ACTION_AppSearch(MSIPACKAGE *package);
extern UINT ACTION_FindRelatedProducts(MSIPACKAGE *package);
extern UINT ACTION_InstallFiles(MSIPACKAGE *package);
extern UINT ACTION_RemoveFiles(MSIPACKAGE *package);
extern UINT ACTION_DuplicateFiles(MSIPACKAGE *package);
extern UINT ACTION_RegisterClassInfo(MSIPACKAGE *package);
extern UINT ACTION_RegisterProgIdInfo(MSIPACKAGE *package);
extern UINT ACTION_RegisterExtensionInfo(MSIPACKAGE *package);
extern UINT ACTION_RegisterMIMEInfo(MSIPACKAGE *package);
extern UINT ACTION_RegisterFonts(MSIPACKAGE *package);

/* Helpers */
extern DWORD deformat_string(MSIPACKAGE *package, LPCWSTR ptr, WCHAR** data );
extern LPWSTR msi_dup_record_field(MSIRECORD *row, INT index);
extern LPWSTR msi_dup_property(MSIPACKAGE *package, LPCWSTR prop);
extern int msi_get_property_int( MSIPACKAGE *package, LPCWSTR prop, int def );
extern LPWSTR resolve_folder(MSIPACKAGE *package, LPCWSTR name, BOOL source,
                      BOOL set_prop, MSIFOLDER **folder);
extern MSICOMPONENT *get_loaded_component( MSIPACKAGE* package, LPCWSTR Component );
extern MSIFEATURE *get_loaded_feature( MSIPACKAGE* package, LPCWSTR Feature );
extern MSIFILE *get_loaded_file( MSIPACKAGE* package, LPCWSTR file );
extern MSIFOLDER *get_loaded_folder( MSIPACKAGE *package, LPCWSTR dir );
extern int track_tempfile(MSIPACKAGE *package, LPCWSTR name, LPCWSTR path);
extern UINT schedule_action(MSIPACKAGE *package, UINT script, LPCWSTR action);
extern LPWSTR build_icon_path(MSIPACKAGE *, LPCWSTR);
extern LPWSTR build_directory_name(DWORD , ...);
extern BOOL create_full_pathW(const WCHAR *path);
extern BOOL ACTION_VerifyComponentForAction(MSICOMPONENT*, INSTALLSTATE);
extern BOOL ACTION_VerifyFeatureForAction(MSIFEATURE*, INSTALLSTATE);
extern void reduce_to_longfilename(WCHAR*);
extern void reduce_to_shortfilename(WCHAR*);
extern LPWSTR create_component_advertise_string(MSIPACKAGE*, MSICOMPONENT*, LPCWSTR);
extern void ACTION_UpdateComponentStates(MSIPACKAGE *package, LPCWSTR szFeature);
extern UINT register_unique_action(MSIPACKAGE *, LPCWSTR);
extern BOOL check_unique_action(MSIPACKAGE *, LPCWSTR);
extern WCHAR* generate_error_string(MSIPACKAGE *, UINT, DWORD, ... );
extern UINT msi_create_component_directories( MSIPACKAGE *package );
extern void msi_ui_error( DWORD msg_id, DWORD type );

/* control event stuff */
extern VOID ControlEvent_FireSubscribedEvent(MSIPACKAGE *package, LPCWSTR event,
                                      MSIRECORD *data);
extern VOID ControlEvent_CleanupSubscriptions(MSIPACKAGE *package);
extern VOID ControlEvent_SubscribeToEvent(MSIPACKAGE *package, msi_dialog *dialog,
                                      LPCWSTR event, LPCWSTR control, LPCWSTR attribute);
extern VOID ControlEvent_UnSubscribeToEvent( MSIPACKAGE *package, LPCWSTR event,
                                      LPCWSTR control, LPCWSTR attribute );

/* User Interface messages from the actions */
extern void ui_progress(MSIPACKAGE *, int, int, int, int);
extern void ui_actiondata(MSIPACKAGE *, LPCWSTR, MSIRECORD *);

/* string consts use a number of places  and defined in helpers.c*/
extern const WCHAR cszSourceDir[];
extern const WCHAR cszSOURCEDIR[];
extern const WCHAR szProductCode[];
extern const WCHAR cszRootDrive[];
extern const WCHAR cszbs[];

/* memory allocation macro functions */
static inline void *msi_alloc( size_t len )
{
    return HeapAlloc( GetProcessHeap(), 0, len );
}

static inline void *msi_alloc_zero( size_t len )
{
    return HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, len );
}

static inline void *msi_realloc( void *mem, size_t len )
{
    return HeapReAlloc( GetProcessHeap(), 0, mem, len );
}

static inline void *msi_realloc_zero( void *mem, size_t len )
{
    return HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, mem, len );
}

static inline BOOL msi_free( void *mem )
{
    return HeapFree( GetProcessHeap(), 0, mem );
}

inline static char *strdupWtoA( LPCWSTR str )
{
    LPSTR ret = NULL;
    DWORD len;

    if (!str) return ret;
    len = WideCharToMultiByte( CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
    ret = msi_alloc( len );
    if (ret)
        WideCharToMultiByte( CP_ACP, 0, str, -1, ret, len, NULL, NULL );
    return ret;
}

inline static LPWSTR strdupAtoW( LPCSTR str )
{
    LPWSTR ret = NULL;
    DWORD len;

    if (!str) return ret;
    len = MultiByteToWideChar( CP_ACP, 0, str, -1, NULL, 0 );
    ret = msi_alloc( len * sizeof(WCHAR) );
    if (ret)
        MultiByteToWideChar( CP_ACP, 0, str, -1, ret, len );
    return ret;
}

inline static LPWSTR strdupW( LPCWSTR src )
{
    LPWSTR dest;
    if (!src) return NULL;
    dest = msi_alloc( (lstrlenW(src)+1)*sizeof(WCHAR) );
    if (dest)
        lstrcpyW(dest, src);
    return dest;
}

#endif /* __WINE_MSI_PRIVATE__ */
