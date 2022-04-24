# Web-Search-Engine-deployed
### Put final_lexicon.txt final_index doctable.txt in the root dir. You can find them here.
### Put msmarco-docs.trec in the root dir. You can download this file by running:
```bash
wget https://msmarco.blob.core.windows.net/msmarcoranking/msmarco-docs.trec.gz
```
### Before running the django, first start the wse, it will load the data about 1 minutes:
```bash
g++ -O3 query.cpp  -std=c++11 -o query_engine
g++ -O3 client.cpp  -std=c++11 -o client
./query_engine msmarco-docs.trec
```
### Then start the django web:
```bash
python3 manage.py makemigrations
python3 manage.py migrate
python3 manage.py runserver
```
