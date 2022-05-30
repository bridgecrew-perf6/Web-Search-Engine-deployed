# Web-Search-Engine-deployed
## Depoly this repo to a web with apache2 on Ubuntu(18.04 or above)
### Please make sure your server has 30GB storage and 4GB memory at least.
### Update your system
```bash
sudo apt-get update
```
### Clone the repo
```bash
cd ~
git clone https://github.com/hangzhangac/Web-Search-Engine-deployed.git
```
### Put final_lexicon.txt final_index doctable.txt in the root dir of this repo. You can download them [here](https://drive.google.com/drive/folders/1lnXNU-8MvvcwOaX4BzAvVrQvs8LMHnbI?usp=sharing). The code to generate these files is [here](https://github.com/hangzhangac/Web-Search-Engine).
###
```bash
mv final_lexicon.txt final_index doctable.txt ~/Web-Search-Engine-deployed
```

### Put msmarco-docs.trec in the root dir of this repo. You can download this file by running:
```bash
wget https://msmarco.blob.core.windows.net/msmarcoranking/msmarco-docs.trec.gz
gunzip msmarco-docs.trec.gz
mv msmarco-docs.trec ~/Web-Search-Engine-deployed
```
### Create the virtual environment for the django
```bash
python3 -m venv ~/Web-Search-Engine-deployed/venv
cd ~/Web-Search-Engine-deployed
source venv/bin/activate
pip install -r requirements.txt 
```
### Config the django settings.
```bash
cd ~/Web-Search-Engine-deployed
vim wse/settings.py
Change the ip address in line 28 to your server's ip address
Then save and quit vim
```
### Install apache2
```bash
cd ~
sudo apt-get install apache2
sudo apt-get install libapache2-mod-wsgi-py3
```
### Config the apache2
```bash
sudo cp ~/Web-Search-Engine-deployed/wse.conf /etc/apache2/sites-available
sudo vim /etc/apache2/sites-available/wse.conf
In line 34,35,39,45 and 46, change the home directory path to yours(Mine is /home/hangzhang)
Then save and quit vim
```
### Enable the apache2 configuration
```bash
cd ~
sudo a2ensite wse
sudo a2dissite 000-default.conf
```
### Change the group 
```bash
cd ~
sudo chown :www-data Web-Search-Engine-deployed/db.sqlite3
sudo chmod 664 Web-Search-Engine-deployed/db.sqlite3
sudo chown :www-data Web-Search-Engine-deployed
sudo chmod 775 Web-Search-Engine-deployed/
sudo chown -R :www-data Web-Search-Engine-deployed/media/
sudo chmod -R 775 Web-Search-Engine-deployed/media/
```

### Allow http
```bash
cd ~
sudo ufw allow http/tcp
sudo service apache2 restart
```

### Change the absolute path in views.py(Never use ~)
```bash
cd ~/Web-Search-Engine-deployed
vim query/views.py
Change all the appearance of the home directory path to yours(Mine is /home/hangzhang), then save and quit vim
vim query.cpp
In line 576 and 581, change the home directory path to yours(Mine is /home/hangzhang), then save and quit vim
```
### Start the engine
```bash
cd ~/Web-Search-Engine-deployed
g++ -O3 query.cpp  -std=c++11 -o query_engine
g++ -O3 client.cpp  -std=c++11 -o client
./query_engine msmarco-docs.trec # Wait patiently, it will load the data for 1-2 minutes:
```
### Restart the apache2
```bash
sudo service apache2 restart
```
### Input your server's ip address on your browser!
