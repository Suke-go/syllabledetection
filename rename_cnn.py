import os

root_dir = r"C:\Users\kosuk\taccel\CNN"

replacements = {
    "　": "_",
    " ": "_",
    "本誌": "magazine_content",
    "音声DL": "audio_download",
    "特集": "Feature",
    "基礎トレーニング": "Basic_Training",
    "ナチュラルスピード": "Natural_Speed",
    "ゆっくりスピード": "Slow_Speed",
    "ポーズ入り": "With_Pauses",
    "ダイジェスト": "Digest",
    "ニュースセレクション": "News_Selection",
    "キーワード": "Keyword",
    "1.5倍速": "1.5x_Speed",
    "CNN スペシャル・インタビュー": "CNN_Special_Interview",
    "ファリード・ザカリアGPS": "Fareed_Zakaria_GPS",
    "この日本語、ネイティブなら何と言う？": "What_would_a_native_say",
    "エンディング・ナレーション": "Ending_Narration",
    "通し音声": "Continuous_Audio",
    "英→日": "Eng_to_Jpn",
    "日→英": "Jpn_to_Eng",
    "News Spotlight": "News_Spotlight",
    "CNN News Focus": "CNN_News_Focus",
    "Talking News": "Talking_News",
    "Stateside Voices": "Stateside_Voices"
}

# Order replacements by length (longest first) to avoid partial matches
# However, for simple dictionary keys, we can just iterate. 
# But to be safe against substring issues (e.g. 'News' vs 'News Spotlight'), length sort is good.
sorted_keys = sorted(replacements.keys(), key=len, reverse=True)

print(f"Starting rename in {root_dir}")

for root, dirs, files in os.walk(root_dir, topdown=False):
    # Rename files first
    for filename in files:
        new_filename = filename
        for key in sorted_keys:
            if key in new_filename:
                new_filename = new_filename.replace(key, replacements[key])
        
        if new_filename != filename:
            old_path = os.path.join(root, filename)
            new_path = os.path.join(root, new_filename)
            # Check if target exists
            if os.path.exists(new_path):
                print(f"Skipping {filename} -> {new_filename} (Target exists)")
                continue

            print(f"Renaming file: {filename} -> {new_filename}")
            try:
                os.rename(old_path, new_path)
            except Exception as e:
                print(f"Error renaming file {filename}: {e}")

    # Rename dirs
    for dirname in dirs:
        new_dirname = dirname
        for key in sorted_keys:
            if key in new_dirname:
                new_dirname = new_dirname.replace(key, replacements[key])
        
        if new_dirname != dirname:
            old_path = os.path.join(root, dirname)
            new_path = os.path.join(root, new_dirname)
            
            if os.path.exists(new_path):
                print(f"Skipping dir {dirname} -> {new_dirname} (Target exists)")
                continue
                
            print(f"Renaming dir: {dirname} -> {new_dirname}")
            try:
                os.rename(old_path, new_path)
            except Exception as e:
                print(f"Error renaming dir {dirname}: {e}")

print("Done.")
