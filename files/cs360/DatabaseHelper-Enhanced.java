package com.example.BridegroomProjectThree;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;


public class DatabaseHelper extends SQLiteOpenHelper {

    private static final String TAG = "DatabaseHelper";
    private static final String DATABASE_NAME = "app.db";
    private static final int DATABASE_VERSION = 2;   // Bumped for schema changes

    // Users table
    private static final String TABLE_USERS = "users";
    private static final String COL_USER_ID = "id";
    private static final String COL_USERNAME = "username";
    private static final String COL_PASSWORD_HASH = "password_hash";
    private static final String COL_SALT = "salt";
    private static final String COL_USER_CREATED = "created_at";

    // Events table
    private static final String TABLE_EVENTS = "events";
    private static final String COL_EVENT_ID = "id";
    private static final String COL_EVENT_TITLE = "title";
    private static final String COL_EVENT_DATETIME = "datetime";
    private static final String COL_EVENT_USER = "username";
    private static final String COL_EVENT_CREATED = "created_at";
    private static final String COL_EVENT_UPDATED = "updated_at";

    public DatabaseHelper(Context context) {
        super(context, DATABASE_NAME, null, DATABASE_VERSION);
    }

    @Override
    public void onConfigure(SQLiteDatabase db) {
        super.onConfigure(db);
        db.setForeignKeyConstraintsEnabled(true);   // Enable foreign keys
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        // Users table with hashed password + salt
        String createUsers = "CREATE TABLE " + TABLE_USERS + " (" +
                COL_USER_ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
                COL_USERNAME + " TEXT UNIQUE NOT NULL, " +
                COL_PASSWORD_HASH + " TEXT NOT NULL, " +
                COL_SALT + " TEXT NOT NULL, " +
                COL_USER_CREATED + " TEXT DEFAULT CURRENT_TIMESTAMP)";
        db.execSQL(createUsers);

        // Events table with foreign key + timestamps
        String createEvents = "CREATE TABLE " + TABLE_EVENTS + " (" +
                COL_EVENT_ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
                COL_EVENT_TITLE + " TEXT NOT NULL, " +
                COL_EVENT_DATETIME + " TEXT NOT NULL, " +
                COL_EVENT_USER + " TEXT NOT NULL, " +
                COL_EVENT_CREATED + " TEXT DEFAULT CURRENT_TIMESTAMP, " +
                COL_EVENT_UPDATED + " TEXT DEFAULT CURRENT_TIMESTAMP, " +
                "FOREIGN KEY(" + COL_EVENT_USER + ") REFERENCES " +
                TABLE_USERS + "(" + COL_USERNAME + ") ON DELETE CASCADE)";
        db.execSQL(createEvents);

        // Index for faster lookups
        db.execSQL("CREATE INDEX idx_events_user_datetime ON " + TABLE_EVENTS +
                " (" + COL_EVENT_USER + ", " + COL_EVENT_DATETIME + ")");
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        if (oldVersion < 2) {
            // Simple migration strategy for this milestone
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_EVENTS);
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_USERS);
            onCreate(db);
        }
    }

    //  Security Helpers

    private String generateSalt() {
        SecureRandom random = new SecureRandom();
        byte[] salt = new byte[16];
        random.nextBytes(salt);
        return bytesToHex(salt);S
    }

    private String hashPassword(String password, String salt) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            String salted = password + salt;
            byte[] hash = digest.digest(salted.getBytes(StandardCharsets.UTF_8));
            return bytesToHex(hash);
        } catch (NoSuchAlgorithmException e) {
            Log.e(TAG, "SHA-256 not available", e);
            throw new RuntimeException("Hashing algorithm unavailable", e);
        }
    }

    private String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }

    private boolean isValidUsername(String username) {
        return username != null && username.trim().length() >= 3 && username.trim().length() <= 30;
    }

    private boolean isValidPassword(String password) {
        return password != null && password.length() >= 6;
    }

    // Users

    public boolean addUser(String username, String password) {
        if (!isValidUsername(username) || !isValidPassword(password)) {
            Log.w(TAG, "Invalid username or password length");
            return false;
        }
        if (usernameExists(username)) {
            return false;
        }

        String salt = generateSalt();
        String hash = hashPassword(password, salt);

        SQLiteDatabase db = this.getWritableDatabase();
        ContentValues cv = new ContentValues();
        cv.put(COL_USERNAME, username.trim());
        cv.put(COL_PASSWORD_HASH, hash);
        cv.put(COL_SALT, salt);

        long result = -1;
        try {
            result = db.insert(TABLE_USERS, null, cv);
        } catch (Exception e) {
            Log.e(TAG, "Error inserting user", e);
        }
        return result != -1;
    }

    public boolean checkUser(String username, String password) {
        if (username == null || password == null) return false;

        SQLiteDatabase db = this.getReadableDatabase();
        Cursor cursor = null;
        try {
            cursor = db.query(TABLE_USERS,
                    new String[]{COL_PASSWORD_HASH, COL_SALT},
                    COL_USERNAME + "=?",
                    new String[]{username.trim()},
                    null, null, null);

            if (cursor.moveToFirst()) {
                String storedHash = cursor.getString(cursor.getColumnIndexOrThrow(COL_PASSWORD_HASH));
                String salt = cursor.getString(cursor.getColumnIndexOrThrow(COL_SALT));
                String computedHash = hashPassword(password, salt);
                return storedHash.equals(computedHash);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error checking user", e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return false;
    }

    public boolean usernameExists(String username) {
        if (username == null) return false;

        SQLiteDatabase db = this.getReadableDatabase();
        Cursor cursor = null;
        try {
            cursor = db.query(TABLE_USERS, new String[]{COL_USER_ID},
                    COL_USERNAME + "=?",
                    new String[]{username.trim()}, null, null, null);
            return cursor.getCount() > 0;
        } catch (Exception e) {
            Log.e(TAG, "Error checking username", e);
            return false;
        } finally {
            if (cursor != null) cursor.close();
        }
    }

    // Events

    public boolean addEvent(String username, String title, String datetime) {
        if (username == null || title == null || datetime == null ||
                title.trim().isEmpty() || datetime.trim().isEmpty()) {
            return false;
        }

        SQLiteDatabase db = this.getWritableDatabase();
        ContentValues cv = new ContentValues();
        cv.put(COL_EVENT_USER, username.trim());
        cv.put(COL_EVENT_TITLE, title.trim());
        cv.put(COL_EVENT_DATETIME, datetime.trim());

        long result = -1;
        try {
            result = db.insert(TABLE_EVENTS, null, cv);
        } catch (Exception e) {
            Log.e(TAG, "Error inserting event", e);
        }
        return result != -1;
    }

    public ArrayList<Event> getEvents(String username) {
        ArrayList<Event> events = new ArrayList<>();
        if (username == null) return events;

        SQLiteDatabase db = this.getReadableDatabase();
        Cursor cursor = null;
        try {
            cursor = db.query(TABLE_EVENTS, null,
                    COL_EVENT_USER + "=?",
                    new String[]{username.trim()},
                    null, null,
                    COL_EVENT_DATETIME + " ASC");

            if (cursor.moveToFirst()) {
                SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());
                do {
                    String title = cursor.getString(cursor.getColumnIndexOrThrow(COL_EVENT_TITLE));
                    String datetime = cursor.getString(cursor.getColumnIndexOrThrow(COL_EVENT_DATETIME));
                    try {
                        Date date = sdf.parse(datetime);
                        events.add(new Event(title, date));
                    } catch (Exception e) {
                        Log.w(TAG, "Failed to parse datetime: " + datetime);
                    }
                } while (cursor.moveToNext());
            }
        } catch (Exception e) {
            Log.e(TAG, "Error retrieving events", e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return events;
    }

    public boolean updateEvent(String username, String oldTitle, String oldDatetime,
                               String newTitle, String newDatetime) {
        if (username == null || oldTitle == null || oldDatetime == null ||
                newTitle == null || newDatetime == null) {
            return false;
        }

        SQLiteDatabase db = this.getWritableDatabase();
        ContentValues cv = new ContentValues();
        cv.put(COL_EVENT_TITLE, newTitle.trim());
        cv.put(COL_EVENT_DATETIME, newDatetime.trim());
        cv.put(COL_EVENT_UPDATED, new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
                .format(new Date()));

        int rows = 0;
        try {
            rows = db.update(TABLE_EVENTS, cv,
                    COL_EVENT_USER + "=? AND " + COL_EVENT_TITLE + "=? AND " + COL_EVENT_DATETIME + "=?",
                    new String[]{username.trim(), oldTitle, oldDatetime});
        } catch (Exception e) {
            Log.e(TAG, "Error updating event", e);
        }
        return rows > 0;
    }

    public boolean deleteEvent(String username, String title, String datetime) {
        if (username == null || title == null || datetime == null) return false;

        SQLiteDatabase db = this.getWritableDatabase();
        int rows = 0;
        try {
            rows = db.delete(TABLE_EVENTS,
                    COL_EVENT_USER + "=? AND " + COL_EVENT_TITLE + "=? AND " + COL_EVENT_DATETIME + "=?",
                    new String[]{username.trim(), title, datetime});
        } catch (Exception e) {
            Log.e(TAG, "Error deleting event", e);
        }
        return rows > 0;
    }


    public boolean deleteAllEventsForUser(String username) {
        if (username == null) return false;
        SQLiteDatabase db = this.getWritableDatabase();
        db.beginTransaction();
        try {
            int rows = db.delete(TABLE_EVENTS, COL_EVENT_USER + "=?", new String[]{username.trim()});
            db.setTransactionSuccessful();
            return rows >= 0;
        } catch (Exception e) {
            Log.e(TAG, "Transaction failed", e);
            return false;
        } finally {
            db.endTransaction();
        }
    }
}