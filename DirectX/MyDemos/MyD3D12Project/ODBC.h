#pragma once

#include "../../Common/d3dUtil.h"
#include "../../Common/GameTimer.h"

#include <atlstr.h>
#include <odbcinst.h>
#include <sqlext.h>

#pragma comment(lib, "odbc32.lib")

const UINT MAX_COUNT = 100;

class ODBC
{
private:
	// INFORMATION for using ODBC tech.
	SQLHANDLE hEnvironment;
	// INFORMATION for using ODBC func. 
	SQLHDBC hODBC;
	// The Variable that stored whether connection is connected or not. (1: Connected)
	char mConnectFlag = 0;

public:
	typedef struct UserData
	{
		UINT id;
		wchar_t target_name[45];
		UINT target_max_number;
		UINT target_number;
		UINT destination_pos_x;
		UINT destination_pos_y;
		UINT limit_time;
		UINT experience;
		UINT hp;
		UINT mp;
		UINT money;
		bool is_clear;
	} UserData;

public:
	bool OnInitDialog()
	{
		SQLSetEnvAttr(
			NULL,
			SQL_ATTR_CONNECTION_POOLING, 
			(SQLPOINTER)SQL_CP_ONE_PER_DRIVER, 
			SQL_IS_INTEGER
		);

		// Composite Environment for using ODBC Tech
		if (SQL_ERROR != SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvironment))
		{
			// Adapted ODBC Version Info
			SQLSetEnvAttr(
				hEnvironment,
				SQL_ATTR_ODBC_VERSION,
				(SQLPOINTER)SQL_OV_ODBC3,
				SQL_IS_INTEGER
			);
			SQLSetEnvAttr(
				hEnvironment,
				SQL_ATTR_CP_MATCH,
				(SQLPOINTER)SQL_CP_RELAXED_MATCH,
				SQL_IS_INTEGER
			);

			// Composite for using ODBC Function
			if (SQL_ERROR != SQLAllocHandle(SQL_HANDLE_DBC, hEnvironment, &hODBC))
			{
				//RETCODE ret_code = SQLConnect(
				//	hODBC,
				//	(SQLWCHAR*)L"game_db", SQL_NTS,
				//	(SQLWCHAR*)L"ldj454", SQL_NTS,
				//	(SQLWCHAR*)L"rainbow970627", SQL_NTS
				//);
				RETCODE ret_code = SQLConnect(
					hODBC,
					(SQLWCHAR*)L"game_db", SQL_NTS,
					(SQLWCHAR*)L"ldj454", SQL_NTS,
					(SQLWCHAR*)L"rainbow970627", SQL_NTS
				);

				// If Accesed into Server is successed.
				if (ret_code == SQL_SUCCESS || ret_code == SQL_SUCCESS_WITH_INFO)
				{
					mConnectFlag = 1;
					return 1;
				}
				// else
				else
				{
					if (hODBC != SQL_NULL_HDBC)
						SQLFreeHandle(SQL_HANDLE_DBC, hODBC);
					if (hEnvironment != SQL_NULL_HENV)
						SQLFreeHandle(SQL_HANDLE_ENV, hEnvironment);
				}
			}
		}
		return true;
	}

	void OnDestroy()
	{
		// If Server is Connected
		if (mConnectFlag == 1)
		{
			// Instance will be Destroied. 
			if (hODBC != SQL_NULL_HDBC)
				SQLFreeHandle(SQL_HANDLE_DBC, hODBC);
			if (hEnvironment != SQL_NULL_HENV)
				SQLFreeHandle(SQL_HANDLE_ENV, hEnvironment);
		}
	}

	void GetTuple (
		UINT PK_ID, 
		UserData& raw_data
	)
	{
		// Load all tuples
		CString query;
		query.Format(L"SELECT * FROM directx_odbc WHERE id = %d", PK_ID);
		// Stored data state
		unsigned short record_state[MAX_COUNT];
		std::string str;

		HSTMT hStatement;
		RETCODE ret_code;

		// Number of Cardinality
		UINT64 record_num = 0;

		// Allocated memory for Query Statements
		if (SQL_SUCCESS == SQLAllocHandle(SQL_HANDLE_STMT, hODBC, &hStatement))
		{
			// TimeOut of Query Event
			SQLSetStmtAttr(hStatement, SQL_ATTR_QUERY_TIMEOUT,
				(SQLPOINTER)15, SQL_IS_UINTEGER);
			// Size of Tuple 
			SQLSetStmtAttr(hStatement, SQL_ATTR_ROW_BIND_TYPE,
				(SQLPOINTER)sizeof(raw_data), 0);
			// Concurrency State
			SQLSetStmtAttr(hStatement, SQL_ATTR_CONCURRENCY,
				(SQLPOINTER)SQL_CONCUR_ROWVER, SQL_IS_UINTEGER);
			SQLSetStmtAttr(hStatement, SQL_ATTR_CURSOR_TYPE,
				(SQLPOINTER)SQL_CURSOR_KEYSET_DRIVEN, SQL_IS_UINTEGER);
			SQLSetStmtAttr(hStatement, SQL_ATTR_ROW_NUMBER,
				(SQLPOINTER)MAX_COUNT, SQL_IS_UINTEGER);
			SQLSetStmtAttr(hStatement, SQL_ATTR_ROW_STATUS_PTR, record_state, 0);
			SQLSetStmtAttr(hStatement, SQL_ATTR_ROWS_FETCHED_PTR, &record_num, 0);

			SQLBindCol(hStatement, 1, SQL_INTEGER, &raw_data.id, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 2, SQL_WCHAR, raw_data.target_name, sizeof(wchar_t) * 45, NULL);
			SQLBindCol(hStatement, 3, SQL_INTEGER, &raw_data.target_max_number, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 4, SQL_INTEGER, &raw_data.target_number, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 5, SQL_INTEGER, &raw_data.destination_pos_x, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 6, SQL_INTEGER, &raw_data.destination_pos_y, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 7, SQL_INTEGER, &raw_data.limit_time, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 8, SQL_INTEGER, &raw_data.experience, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 9, SQL_INTEGER, &raw_data.hp, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 10, SQL_INTEGER, &raw_data.mp, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 11, SQL_INTEGER, &raw_data.money, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 12, SQL_TINYINT, &raw_data.is_clear, sizeof(bool), NULL);

			ret_code = SQLExecDirect(hStatement, (SQLWCHAR*)(const wchar_t *)query, SQL_NTS);

			// push back new tuple.
			while (ret_code = SQLFetchScroll(hStatement, SQL_FETCH_NEXT, 0) != SQL_NO_DATA)
			{
				for (UINT i = 0; i < record_num; i++)
				{
					if (record_state[i] != SQL_ROW_DELETED && record_state[i] != SQL_ROW_ERROR)
					{
						
					}
				}
			}

			SQLFreeHandle(SQL_HANDLE_STMT, hStatement);
		}
	}

	void GetCurrentQuest(
		UserData& raw_data
	)
	{
		// Load all tuples
		CString query;
		query.Format(L"SELECT * FROM directx_odbc WHERE is_clear = 0");
		// Stored data state
		unsigned short record_state[MAX_COUNT];
		std::string str;

		HSTMT hStatement;
		RETCODE ret_code;

		// Number of Cardinality
		UINT64 record_num = 0;

		// Allocated memory for Query Statements
		if (SQL_SUCCESS == SQLAllocHandle(SQL_HANDLE_STMT, hODBC, &hStatement))
		{
			// TimeOut of Query Event
			SQLSetStmtAttr(hStatement, SQL_ATTR_QUERY_TIMEOUT,
				(SQLPOINTER)15, SQL_IS_UINTEGER);
			// Size of Tuple 
			SQLSetStmtAttr(hStatement, SQL_ATTR_ROW_BIND_TYPE,
				(SQLPOINTER)sizeof(raw_data), 0);
			// Concurrency State
			SQLSetStmtAttr(hStatement, SQL_ATTR_CONCURRENCY,
				(SQLPOINTER)SQL_CONCUR_ROWVER, SQL_IS_UINTEGER);
			SQLSetStmtAttr(hStatement, SQL_ATTR_CURSOR_TYPE,
				(SQLPOINTER)SQL_CURSOR_KEYSET_DRIVEN, SQL_IS_UINTEGER);
			SQLSetStmtAttr(hStatement, SQL_ATTR_ROW_NUMBER,
				(SQLPOINTER)MAX_COUNT, SQL_IS_UINTEGER);
			SQLSetStmtAttr(hStatement, SQL_ATTR_ROW_STATUS_PTR, record_state, 0);
			SQLSetStmtAttr(hStatement, SQL_ATTR_ROWS_FETCHED_PTR, &record_num, 0);

			SQLBindCol(hStatement, 1, SQL_INTEGER, &raw_data.id, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 2, SQL_WCHAR, raw_data.target_name, sizeof(wchar_t) * 45, NULL);
			SQLBindCol(hStatement, 3, SQL_INTEGER, &raw_data.target_max_number, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 4, SQL_INTEGER, &raw_data.target_number, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 5, SQL_INTEGER, &raw_data.destination_pos_x, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 6, SQL_INTEGER, &raw_data.destination_pos_y, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 7, SQL_INTEGER, &raw_data.limit_time, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 8, SQL_INTEGER, &raw_data.experience, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 9, SQL_INTEGER, &raw_data.hp, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 10, SQL_INTEGER, &raw_data.mp, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 11, SQL_INTEGER, &raw_data.money, sizeof(UINT), NULL);
			SQLBindCol(hStatement, 12, SQL_TINYINT, &raw_data.is_clear, sizeof(bool), NULL);

			ret_code = SQLExecDirect(hStatement, (SQLWCHAR*)(const wchar_t *)query, SQL_NTS);

			// push back new tuple.
			while (ret_code = SQLFetchScroll(hStatement, SQL_FETCH_NEXT, 0) != SQL_NO_DATA)
			{
				for (UINT i = 0; i < record_num; i++)
				{
					if (record_state[i] != SQL_ROW_DELETED && record_state[i] != SQL_ROW_ERROR)
					{

					}
				}

				break;
			}

			SQLFreeHandle(SQL_HANDLE_STMT, hStatement);
		}
	}

	void InsertTuple(UserData& data)
	{
		CString query;

		query = L"INSERT INTO directx_odbc VALUES ('%d', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d')",
			data.id,
			data.target_name,
			data.target_max_number,
			data.target_number,
			data.destination_pos_x,
			data.destination_pos_y,
			data.limit_time,
			data.experience,
			data.hp,
			data.mp,
			data.money,
			data.is_clear;

		SQLHSTMT hStatement;
		// Alocated Memory for Query
		if (SQL_SUCCESS == SQLAllocHandle(SQL_HANDLE_STMT, hODBC, &hStatement))
		{
			// Set TimeOut for Query
			SQLSetStmtAttr(hStatement, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)15, SQL_IS_UINTEGER);

			// Execute SQL Query
			RETCODE ret = SQLExecDirect(hStatement, (SQLWCHAR*)(const wchar_t *)query, SQL_NTS);

			if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
			{
				// TODO
			}

			// Successful Querying Commit
			SQLEndTran(SQL_HANDLE_ENV, hEnvironment, SQL_COMMIT);
			// Free Query Memory
			SQLFreeHandle(SQL_HANDLE_STMT, hStatement);
		}
	}

	void DeleteTuple(UINT ID)
	{
		CString query;

		query.Format(L"DELETE FROM directx_odbc WHERE id = %d", ID);

		SQLHSTMT hStatement;
		// Alocated Memory for Query
		if (SQL_SUCCESS == SQLAllocHandle(SQL_HANDLE_STMT, hODBC, &hStatement))
		{
			// Set TimeOut for Query
			SQLSetStmtAttr(hStatement, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)15, SQL_IS_UINTEGER);

			// Execute SQL Query
			RETCODE ret = SQLExecDirect(hStatement, (SQLWCHAR*)(const wchar_t *)query, SQL_NTS);

			if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
			{
				// TODO
			}

			// Successful Querying Commit
			SQLEndTran(SQL_HANDLE_ENV, hEnvironment, SQL_COMMIT);
			// Free Query Memory
			SQLFreeHandle(SQL_HANDLE_STMT, hStatement);
		}
	}

	void SetComplete(UINT ID)
	{
		CString query;

		query.Format(L"UPDATE directx_odbc SET is_clear = 1 WHERE ID = %d", ID);

		SQLHSTMT hStatement;
		// Alocated Memory for Query
		if (SQL_SUCCESS == SQLAllocHandle(SQL_HANDLE_STMT, hODBC, &hStatement))
		{
			// Set TimeOut for Query
			SQLSetStmtAttr(hStatement, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)15, SQL_IS_UINTEGER);

			// Execute SQL Query
			RETCODE ret = SQLExecDirect(hStatement, (SQLWCHAR*)(const wchar_t *)query, SQL_NTS);

			if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
			{
				// TODO
			}

			// Successful Querying Commit
			SQLEndTran(SQL_HANDLE_ENV, hEnvironment, SQL_COMMIT);
			// Free Query Memory
			SQLFreeHandle(SQL_HANDLE_STMT, hStatement);
		}
	}
};