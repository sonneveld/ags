package com.bigbluecup.android.launcher;

import com.bigbluecup.android.AgsEngine;
import com.bigbluecup.android.CreditsActivity;
import com.bigbluecup.android.EngineGlue;
import com.bigbluecup.android.PreferencesActivity;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;

import android.Manifest;
import android.app.AlertDialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Environment;
import android.text.Editable;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.Toast;

import androidx.core.app.ActivityCompat;

public class GamesList extends ListActivity
{
	private static final int AGS_PERMISSION_TO_BUILD_GAMES_LIST = 683;

	String filename = null;
	@SuppressWarnings("unused")
	private ProgressDialog dialog;

	private ArrayList<String> folderList;
	private ArrayList<String> filenameList;
	
	private String baseDirectory;
	
	private void showMessage(String message)
	{
		Toast.makeText(this, message, Toast.LENGTH_LONG).show();
	}
	
	public void onCreate(Bundle bundle)
	{
		super.onCreate(bundle);
		
		// Get base directory from data storage
		SharedPreferences settings = getSharedPreferences("gameslist", 0);
		baseDirectory = settings.getString("baseDirectory", Environment.getExternalStorageDirectory().getAbsolutePath() + "/ags");

//		if (!baseDirectory.startsWith("/"))
//			baseDirectory = "/" + baseDirectory;

		// Build game list
		buildGamesListWithPermission();
		
		registerForContextMenu(getListView());
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		int id = item.getItemId();
		if (id == R.id.credits)
		{
			showCredits();
			return true;
		}
		else if (id == R.id.preferences)
		{
			showPreferences(-1);
			return true;
		}
		else if (id == R.id.setfolder)
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(this);

			alert.setTitle("Game folder");

			final EditText input = new EditText(this);
			input.setText(baseDirectory);
			alert.setView(input);

			alert.setPositiveButton("Ok", new DialogInterface.OnClickListener()
			{
				public void onClick(DialogInterface dialog, int whichButton)
				{
					Editable value = input.getText();
					baseDirectory = value.toString();
					// The launcher will accept paths that don't start with a slash, but
					// the engine cannot find the game later.
					if (!baseDirectory.startsWith("/"))
						baseDirectory = "/" + baseDirectory;
					buildGamesListWithPermission();
				}
			});
				
			alert.setNegativeButton("Cancel", new DialogInterface.OnClickListener()
			{
				public void onClick(DialogInterface dialog, int whichButton)
				{
				}
			});

			alert.show();
			
		}
		
		return super.onOptionsItemSelected(item);
	}
	
	@Override
	protected void onStop()
	{
		super.onStop();

		SharedPreferences settings = getSharedPreferences("gameslist", 0);
		SharedPreferences.Editor editor = settings.edit();
		editor.putString("baseDirectory", baseDirectory);
		editor.commit();
	}

	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo)
	{
		super.onCreateContextMenu(menu, v, menuInfo);
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.game_context_menu, menu);
		
		menu.setHeaderTitle(folderList.get((int)((AdapterContextMenuInfo)menuInfo).id));
	}	
	
	@Override
	public boolean onContextItemSelected(MenuItem item)
	{
		AdapterContextMenuInfo info = (AdapterContextMenuInfo)item.getMenuInfo();
		if (item.getItemId() == R.id.preferences) {
			showPreferences((int)info.id);
			return true;
		} else if (item.getItemId() == R.id.start) {
			startGame(filenameList.get((int)info.id), false);
			return true;
		} else if (item.getItemId() == R.id.continueGame) {
			startGame(filenameList.get((int)info.id), true);
			return true;
		} else {
			return super.onContextItemSelected(item);
		}
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id)
	{
		super.onListItemClick(l, v, position, id);
		
		startGame(filenameList.get(position), false);
	}

	private void showCredits()
	{
		Intent intent = new Intent(this, CreditsActivity.class);
		startActivity(intent);
	}
	
	private void showPreferences(int position)
	{
		Intent intent = new Intent(this, PreferencesActivity.class);
		Bundle b = new Bundle();
		b.putString("name", (position < 0) ? "" : folderList.get(position));
		b.putString("filename", (position < 0) ? null : filenameList.get(position));
		b.putString("directory", baseDirectory);
		intent.putExtras(b);
		startActivity(intent);
	}
	
	private void startGame(String filename, boolean loadLastSave)
	{
		Intent intent = new Intent(this, AgsEngine.class);
		Bundle b = new Bundle();
		b.putString("filename", filename);
		b.putString("directory", baseDirectory);
		b.putBoolean("loadLastSave", loadLastSave);
		intent.putExtras(b);
		startActivity(intent);
		finish();
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
	{
		switch (requestCode) {
			case AGS_PERMISSION_TO_BUILD_GAMES_LIST: {
				if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
					buildGamesList();
				}
				return;
			}

			default:
			// other 'case' lines to check for other
			// permissions this app might request.
		}
	}

	private void buildGamesListWithPermission()
	{
		int check = ActivityCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE);
		if (check != PackageManager.PERMISSION_GRANTED) {
			ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, AGS_PERMISSION_TO_BUILD_GAMES_LIST);
			// buildGamesList to be called again when permission granted.
			return;
		} else {
			buildGamesList();
		}
	}

	private void buildGamesList()
	{
		filename = searchForGames();
		
		if (filename != null)
			startGame(filename, false);
		
		if ((folderList != null) && (folderList.size() > 0))
		{
			this.setListAdapter(new ArrayAdapter<String>(this, android.R.layout.simple_list_item_1, folderList));
		}
		else
		{
			this.setListAdapter(null);
			showMessage("No games found in \"" + baseDirectory + "\"");
		}
	}
	
	private String searchForGames()
	{
		String[] tempList = null;

		folderList = null;
		filenameList = null;
		
		// Check for ac2game.dat in the base directory
		String ac2game_path = EngineGlue.FindGameDataInDirectory(baseDirectory);
		if (ac2game_path != null) {
			File ac2game = new File(ac2game_path);
			if (ac2game.isFile())
				return baseDirectory + "/ac2game.dat";
		}

		
		// Check for games in folders
		File agsDirectory = new File(baseDirectory);
		if (agsDirectory.isDirectory())
		{
			tempList = agsDirectory.list(new FilenameFilter()
			{
				public boolean accept(File dir, String filename)
				{
					return new File(dir + "/" + filename).isDirectory();
				}
			});
		}
		
		if (tempList != null)
		{
			java.util.Arrays.sort(tempList);
			
			folderList = new ArrayList<String>();
			filenameList = new ArrayList<String>();
			
			int i;
			for (i = 0; i < tempList.length; i++)
			{
				String agsgame_path = EngineGlue.FindGameDataInDirectory(agsDirectory + "/" + tempList[i]);
				if (agsgame_path != null) {
					File agsgame_file = new File(agsgame_path);
					if (agsgame_file.isFile()) {
						folderList.add(tempList[i]);
						filenameList.add(agsgame_path);
					}
				}
			}
		}
		
		return null;
	}

	// Prevent the activity from being destroyed on a configuration change
	@Override
	public void onConfigurationChanged(Configuration newConfig)
	{
		super.onConfigurationChanged(newConfig);
	}
}
