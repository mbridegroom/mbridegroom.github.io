package com.example.BridegroomProjectThree;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;

public class DatabaseHelper extends SQLiteOpenHelper {

    private static final String DATABASE_NAME = "app.db";
    private static final int DATABASE_VERSION = 1;

    // Users table
    private static final String TABLE_USERS = "users";
    private static final String COL_USER_ID = "id";
    private static final String COL_USERNAME = "username";
    private static final String COL_PASSWORD = "password";

    // Events table
    private static final String TABLE_EVENTS = "events";
    private static final String COL_EVENT_ID = "id";
    private static final String COL_EVENT_TITLE = "title";
    private static final String COL_EVENT_DATETIME = "datetime";
    private static final String COL_EVENT_USER = "username";

    public DatabaseHelper(Context context) {
        super(context, DATABASE_NAME, null, DATABASE_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        String createUsers = "CREATE TABLE " + TABLE_USERS + " (" +
                COL_USER_ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                COL_USERNAME + " TEXT UNIQUE," +
                COL_PASSWORD + " TEXT)";
        db.execSQL(createUsers);

        String createEvents = "CREATE TABLE " + TABLE_EVENTS + " (" +
                COL_EVENT_ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                COL_EVENT_TITLE + " TEXT," +
                COL_EVENT_DATETIME + " TEXT," +
                COL_EVENT_USER + " TEXT)";
        db.execSQL(createEvents);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        db.execSQL("DROP TABLE IF EXISTS " + TABLE_USERS);
        db.execSQL("DROP TABLE IF EXISTS " + TABLE_EVENTS);
        onCreate(db);
    }

    // --- Users ---
    public boolean addUser(String username, String password) {
        SQLiteDatabase db = this.getWritableDatabase();
        ContentValues cv = new ContentValues();
        cv.put(COL_USERNAME, username);
        cv.put(COL_PASSWORD, password);
        long result = db.insert(TABLE_USERS, null, cv);
        return result != -1;
    }

    public boolean checkUser(String username, String password) {
        SQLiteDatabase db = this.getReadableDatabase();
        Cursor cursor = db.query(TABLE_USERS, null,
                COL_USERNAME + "=? AND " + COL_PASSWORD + "=?",
                new String[]{username, password}, null, null, null);
        boolean exists = cursor.getCount() > 0;
        cursor.close();
        return exists;
    }

    public boolean usernameExists(String username) {
        SQLiteDatabase db = this.getReadableDatabase();
        Cursor cursor = db.query(TABLE_USERS, null,
                COL_USERNAME + "=?",
                new String[]{username}, null, null, null);
        boolean exists = cursor.getCount() > 0;
        cursor.close();
        return exists;
    }

    // --- Events ---
    public boolean addEvent(String username, String title, String datetime) {
        SQLiteDatabase db = this.getWritableDatabase();
        ContentValues cv = new ContentValues();
        cv.put(COL_EVENT_USER, username);
        cv.put(COL_EVENT_TITLE, title);
        cv.put(COL_EVENT_DATETIME, datetime);
        long result = db.insert(TABLE_EVENTS, null, cv);
        return result != -1;
    }

    public ArrayList<Event> getEvents(String username) {
        ArrayList<Event> events = new ArrayList<>();
        SQLiteDatabase db = this.getReadableDatabase();
        Cursor cursor = db.query(TABLE_EVENTS, null,
                COL_EVENT_USER + "=?",
                new String[]{username}, null, null, COL_EVENT_DATETIME + " ASC");
        if (cursor.moveToFirst()) {
            do {
                String title = cursor.getString(cursor.getColumnIndexOrThrow(COL_EVENT_TITLE));
                String datetime = cursor.getString(cursor.getColumnIndexOrThrow(COL_EVENT_DATETIME));
                try {
                    Date date = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault()).parse(datetime);
                    events.add(new Event(title, date));
                } catch (Exception e) {
                    e.printStackTrace();
                }
            } while (cursor.moveToNext());
        }
        cursor.close();
        return events;
    }

    public boolean updateEvent(String username, String oldTitle, String oldDatetime, String newTitle, String newDatetime) {
        SQLiteDatabase db = this.getWritableDatabase();
        ContentValues cv = new ContentValues();
        cv.put(COL_EVENT_TITLE, newTitle);
        cv.put(COL_EVENT_DATETIME, newDatetime);
        int rows = db.update(TABLE_EVENTS, cv,
                COL_EVENT_USER + "=? AND " + COL_EVENT_TITLE + "=? AND " + COL_EVENT_DATETIME + "=?",
                new String[]{username, oldTitle, oldDatetime});
        return rows > 0;
    }

    public boolean deleteEvent(String username, String title, String datetime) {
        SQLiteDatabase db = this.getWritableDatabase();
        int rows = db.delete(TABLE_EVENTS,
                COL_EVENT_USER + "=? AND " + COL_EVENT_TITLE + "=? AND " + COL_EVENT_DATETIME + "=?",
                new String[]{username, title, datetime});
        return rows > 0;
    }
}