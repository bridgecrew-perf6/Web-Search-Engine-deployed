/*************************************************************************
	> File Name: query.cpp
    > Full Path: /Users/zhanghang/workspace/WSE/assign2_2/query.cpp
	> Author: Hang Zhang
	> Created Time: 10/24 13:31:59 2021
 ************************************************************************/

#include<iostream>
#include<algorithm>
#include<cstdlib>
#include<cstring>
#include<cstdio>
#include<stack>
#include<string>
#include<queue>
#include<set>
#include<map>
#include<cmath>
#include<fstream>
#include<unordered_map>
#include<assert.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
//using namespace std;
const int MAX_DOCID = 3213835;
std::ofstream fout;
std::unordered_map<int,std::tuple<std::string, int, long long> >doctable; //document table, the value is <url, term number, text start offset in trec file>
std::unordered_map<std::string,std::tuple<long long, long long, int> >lexicon;//lexicon, the value is<startoffset, endoffset, document number>
//double ave_length_d = 1.0; // the average length of documents in the collection
//string trec_path = "../assign2_data/msmarco-docs.trec";
//string lexicon_path = "final_lexicon.txt", doctable_path = "doctable.txt", index_path = "final_index";

//load stopwords
std::set<std::string> load_stopping_words(std::string stop_words_filepath){
	std::string line;
	std::set<std::string>stopwords;
	std::ifstream infile(stop_words_filepath);
	while(getline(infile,line)){
		stopwords.insert(line);
	}
	return stopwords;
}

//split the text by delim 
std::vector<std::string> split(const std::string& str, const std::string& delim, std::set<std::string>&stopwords) {
	std::vector<std::string> res;
	if("" == str) return res;
	char * strs = new char[str.length() + 1]; 
	strcpy(strs, str.c_str());

	char * d = new char[delim.length() + 1];
	strcpy(d, delim.c_str());

	char *p = strtok(strs, d);
	while(p) {
		std::string s = p;
		//if(stopwords.find(s)==stopwords.end())
		res.push_back(s); 
		p = strtok(NULL, d);
	}
	delete[] d;
	delete[] strs;
	return res;
}

// load the lexicon and document table from disk
double readin(std::string lexicon_path, std::string doctable_path){
	clock_t start,end;
	start = clock();
	std::ifstream infile;
	infile.open(lexicon_path);
	std::string line;
	std::set<std::string>empty_stopwords;
	while(getline(infile,line)){
		std::vector<std::string>res = split(line,"\t",empty_stopwords);
		assert(res.size()==4);
		lexicon[res[0]]=std::tuple<long long, int, int>(stoll(res[1]),stoi(res[2]),stoi(res[3]));
	}
	infile.close();
	long long length = 0;
	infile.open(doctable_path);
	while(getline(infile,line)){
		std::vector<std::string>res = split(line,"\t",empty_stopwords);
		assert(res.size()==4);
		doctable[stoi(res[0])]=std::tuple<std::string, int, long long>(res[1],stoi(res[2]),stoll(res[3]));
		length+=stoi(res[2]);
	}
	double ave_length_d = length/doctable.size();
	infile.close();
	std::cout<<"The size of lexicon: "<<lexicon.size()<<std::endl<<"The size of docment table: "<<doctable.size()<<std::endl;
	std::cout<<"Loading lexicon and document table finished!"<<std::endl;
	end = clock();
	std::cout<<"The running time of Loading is "<<(double)(end-start)/CLOCKS_PER_SEC<<" seconds"<<std::endl;
	return ave_length_d;
}

//compute BM25 score for a document
double compute_BM25(int N, int ft, int f_d_t, int length_d, double ave_length_d){
	double ans=0.0;
	const double k1 = 1.2, b=0.75;
	const double IDF = log2((N-ft+0.5)/(ft+0.5));
	const double K = k1*((1-b)+b*length_d/ave_length_d);
	double S = (k1+1)*f_d_t/(K+f_d_t);
	ans = IDF*S;
	return ans;
}

class InvertedList{
public:
	std::string term;
	unsigned char *buffer;// the buffer stroing all 
	std::vector<int>lastdocid;// last document id in metadata
	std::vector<int>block_offset;// block offset in metadata
	int buffer_length;// the length of the buffer
	int buffer_id;
	int cur_block; // the current block where we are in
	int overall_doc_num; // the number of documents in this inverted list
	int block_num;// the number of blocks in this inverted list
	int last_block_num;// denote the number of documents in the last block
	std::vector<int>cur_docids; // decompress one block and cache the document ids in memory
	std::vector<int>cur_freqs; // decompress one block and cache the frequency in memory
	int cur_id; // the pointer shared by nextGEQ and getFreq
	int sum;// prefix sum of current block
	

	InvertedList(){
		buffer = nullptr;
		buffer_length = buffer_id = cur_block = overall_doc_num = block_num = last_block_num = cur_id = sum = 0;
		cur_block = -1;
	}
	bool openList(std::string term,std::string index_path = "final_index"){
		this->term=term;
		std::ifstream infile;
		infile.open(index_path);
		auto info = lexicon[term];
		long long offset_start = std::get<0>(info);
		//long long offset_end = std::get<1>(info);
		overall_doc_num = std::get<2>(info);
		buffer_length = std::get<1>(info);
//		std::cout<<"The length of the inverted list of <"<<term<<"> is "<<buffer_length<<std::endl;
		if(buffer_length==0)return false;
		block_num = (overall_doc_num-1)/64+1;
		
		buffer = new unsigned char [buffer_length];
		infile.seekg(offset_start,std::ios::beg);
		infile.read((char*)buffer,buffer_length); //read the inverted list for the term
		infile.close();

		last_block_num = overall_doc_num%64;
		if(last_block_num==0)last_block_num=64; //calculate the number of documents in the last block

		buffer_id=0;
		cal_metadata(buffer_id); //read metadata from buffer

		//delete the compressed metadata from buffer 
		unsigned char* tmp_buffer = new unsigned char [buffer_length-buffer_id];
		for(int i=0,j=buffer_id;j<buffer_length;i++,j++){
			tmp_buffer[i]=buffer[j];
		}
		delete [] buffer;
		buffer = tmp_buffer;
		tmp_buffer = nullptr;
		buffer_length -= buffer_id;
		buffer_id=0;
		return true;
	}
	int nextGEQ(int k){
		bool change = false; //will not swith to the next block

		//find the block which docutment k may potentially be in
		while((cur_block==-1)||(cur_block<block_num&&lastdocid[cur_block]<k)){ 
			cur_block++;
			change = true; // swith to another block
			cur_docids.clear(); // Since this block will not be used anymore, free the momery for this block
			cur_freqs.clear();
		}
		if(cur_block==block_num){ //Did not find a block which may contain k
			return MAX_DOCID+1;
		}
		if(change){
			int start_offset = block_offset[cur_block];
			int num_doc_in_block = cur_block==block_num-1? last_block_num :64;

			//decompress all docutment ids and frequencies in the block
			auto res = read_2n_number_from_buffer(start_offset, num_doc_in_block); 
			cur_docids = res.first; //cache it in memory
			cur_freqs = res.second; //cache it in memory
			cur_id=0; // the index of document ids cached in memory
			sum = cur_block-1>=0? lastdocid[cur_block-1]:0; // the prefix sum
		}

		// find the document id k in the current block
		while(cur_id<cur_docids.size()&&cur_docids[cur_id]+sum<k){
			sum+=cur_docids[cur_id];
			cur_id++;
		}
		if(cur_id==cur_docids.size())return MAX_DOCID+1;// Did not find it
		return sum+cur_docids[cur_id];
	}
	int getFreq(int k){ //get the frequency for document whose id is k
		assert(cur_docids[cur_id]+sum==k);
		return cur_freqs[cur_id];
	}

	// decompress the whole block
	std::pair< std::vector<int>,std::vector<int> > read_2n_number_from_buffer(int offset, int n){
		int cnt=0;
		int buffer_id = offset;
		std::vector<int>docid;
		while(cnt<n){
			docid.push_back(VarDecode(buffer_id));
			cnt++;
		}
		std::vector<int>freq;
		while(cnt<2*n){
			freq.push_back(VarDecode(buffer_id));
			cnt++;
		}
		return {docid,freq};
	}
	
	//extract metadata from buffer
	void cal_metadata(int &buffer_id){
		int cnt=0;
		while(buffer_id<buffer_length&&cnt<block_num){
			lastdocid.push_back(VarDecode(buffer_id));
			cnt++;
		}
		int prefix_sum=0;
		while(buffer_id<buffer_length&&cnt<block_num*2){
			block_offset.push_back(prefix_sum);
			int tmp = VarDecode(buffer_id);
			prefix_sum+=tmp;
			cnt++;
		}
		//cout<<"current buffer id: "<<buffer_id<<" buffer length: "<<buffer_length<<""<<endl;
	}
	int VarDecode(int &i){
		int val=0,shift=0;
		int b;
		while(1){
			b=buffer[i];
			if(b>=128)break;
			val+=b<<shift;
			shift+=7;
			i++;
		}
		val+=((b-128)<<shift);
		i++;
		return val;
	}
	void closeList(){ //close the list
		delete [] buffer;
		buffer=nullptr;
		lastdocid.clear();
		block_offset.clear();
		cur_docids.clear();
		cur_freqs.clear();
	}

	//TAAT method for disjunctive query
	void TAAT(std::unordered_map<int,double>&bm25, std::string term, std::unordered_map<int,std::map<std::string,double>>&docid_term_score, double ave_length_d){
		while(cur_block<block_num){
			if(cur_block==-1){
				cur_block++;
				continue;
			}
			int start_offset = block_offset[cur_block];
			int num_doc_in_block = cur_block==block_num-1? last_block_num :64;
			auto res = read_2n_number_from_buffer(start_offset, num_doc_in_block); 
			cur_docids = res.first;
			cur_freqs = res.second;
			cur_id=0;
			sum = cur_block-1>=0? lastdocid[cur_block-1]:0;
			while(cur_id<cur_docids.size()){
				int cur_docid = sum + cur_docids[cur_id];
				int cur_freq = cur_freqs[cur_id];
				double cur_score = compute_BM25((int)doctable.size(), overall_doc_num, cur_freq, std::get<1>(doctable[cur_docid]), ave_length_d);
				docid_term_score[cur_docid][term]+=cur_score;
				bm25[cur_docid] += cur_score;
				sum+=cur_docids[cur_id];
				cur_id++;
			}
			cur_block++;
		}
		return;
	}

	~InvertedList(){
		//if(buffer!=nullptr)delete [] buffer;
	}
};

class Query{
public:
	std::string delimiters;
	std::set<std::string>stopwords;
	double ave_length_d;
	std::string trec_path;
	std::string index_path;
	Query(std::string trec_path, std::string index_path, double ave_length_d){
		this->ave_length_d = ave_length_d;
		this->trec_path = trec_path;
		this->index_path = index_path;
		delimiters = "\t ,.?#$%():;^*/!-\'\"=><·+~";
		stopwords = load_stopping_words("stopping_words.txt");
		ave_length_d = 1.0;
	}
	
	//sort the InvertedList by the number of documents in the list
	static bool cmp(InvertedList l1, InvertedList l2){
		return l1.overall_doc_num < l2.overall_doc_num;
	}
	
	// Get the top 10 (defaultly) result by BM25 score for DAAT method for conjunctive query
	std::vector<std::pair<double,int>> Top_result(std::vector<std::pair<double, int>>&BM25_docid,int return_number){
		std::vector<std::pair<double,int>> res;
		std::priority_queue< std::pair<double,int>, std::vector< std::pair<double,int> >, std::greater< std::pair<double,int> >  >pq;
		for(int i=0;i<BM25_docid.size();i++){
			pq.push(BM25_docid[i]);
			if(pq.size()>return_number)pq.pop();
		}
		while(!pq.empty()){
			res.push_back(pq.top());pq.pop();
		}
		reverse(res.begin(),res.end());
		return res;
	}
	
	// Get the top 10 (defaultly) result by BM25 score for TAAT method for disjunctive query
	std::vector<std::pair<double,int>> Top_result(std::unordered_map<int,double>&bm25,int return_number/*, unordered<int, map<string,double>>&docid_term_score*/){
		std::vector<std::pair<double,int>> res;
		std::priority_queue< std::pair<double,int>, std::vector< std::pair<double,int> >, std::greater< std::pair<double,int> >  >pq;
		for(auto &x:bm25){
			pq.push({x.second,x.first});
			if(pq.size()>return_number)pq.pop();
		}
		while(!pq.empty()){
			res.push_back(pq.top());pq.pop();
		}
		reverse(res.begin(),res.end());
		return res;
	}
	
	static bool equal_char(char a, char b){
		if(a>='A'&&a<='Z')a+=32;
		if(b>='A'&&b<='Z')b+=32;
		return a==b;
	}
	static void output_stright_line(){
		fout<<"------------------------------------------------------------------------"<<std::endl;
	}
	//snippet generation
	void snippet_generation(std::vector<std::pair<double,int>> &top_result, std::vector<std::string>&terms, std::string delimiters, std::unordered_map<int,std::map<std::string,double>>&docid_term_score){
		std::ifstream infile(trec_path);
		std::set<char> d(delimiters.begin(), delimiters.end());
		for(int top_result_id=0;top_result_id<top_result.size();top_result_id++){
			auto &x = top_result[top_result_id];
			int did = x.second;
			long long start_offset = std::get<2>(doctable[did]); //get the start offset of the document in trec file
			infile.seekg(start_offset,std::ios_base::beg);
			std::string line;
			//output document id, bm25 score and url
			output_stright_line();
//			std::cout<<"Document id: "<<did<<"\t"<<"Document length: "<<std::get<1>(doctable[did])<<"\t"<<"BM25 score: "<<x.first<<std::endl;
			std::map<std::string,double>&terms_score = docid_term_score[did];
//			for(auto &x:terms_score)std::cout<<x.first<<" "<<x.second<<"\t";
//			std::cout<<std::endl;
			
			fout<<"Url: "<<std::get<0>(doctable[did])<<std::endl;
			fout<<"Snippet(Context): ";
			//output snippet
			std::string snippet_line = "";
			int number_diff_appearance = 0, number_appearance =0 ;
			int begin_index = 0;
			while(getline(infile,line)){	
				if(line=="</TEXT>")break;
				std::set<int>insert_begin_position,insert_end_positon;
				int different_terms = 0;
				for(auto &term:terms){
					bool showup=false;
					for(int i=0;i<line.size();i++){
						int k=i,j=0;
						while(j<term.size()&&k<line.size()&&equal_char(line[k],term[j]))k++,j++;
						if(j!=term.size())continue;
						if((i-1<0||d.find(line[i-1])!=d.end())&&(k==line.size()||d.find(line[k])!=d.end())){
							insert_begin_position.insert(i);//record the position we will insert highlight symbol
							insert_end_positon.insert(k-1);// record the position we will insert the highlight end symbol
							if(!showup)different_terms+=(showup=true);
						}
					}
				}
				bool show = true;
				//if(insert_begin_position.size()<2 ||(!show)||different_terms<number_appearance)continue;
				if((!show)||different_terms<number_diff_appearance)continue;
				if(different_terms==number_diff_appearance&&insert_begin_position.size()<number_appearance)continue;
				std::string new_line = "";
				std::string start_color = "", end_color = ""; //to highlight the terms
				for(int i=0;i<line.size();i++){
					if(insert_begin_position.find(i)!=insert_begin_position.end())new_line+=start_color;
					new_line+=line[i];
					if(insert_end_positon.find(i)!=insert_end_positon.end())new_line+=end_color;
				}
				//cout<<new_line<<endl;
				number_diff_appearance = different_terms;
				number_appearance = insert_begin_position.size();
				snippet_line = new_line;
				begin_index = *insert_begin_position.begin();
			}
			if(snippet_line.size()>2000)
				snippet_line = (begin_index==0 ? "" : "...") + snippet_line.substr(begin_index,std::min(2000,(int)snippet_line.size()-begin_index))+"...";
			fout<<snippet_line<<std::endl;
		}
		output_stright_line();
		infile.close();
		return;
	}
	
	// TAAT method for disjunctive query
	void disjunctive(std::vector<std::string>terms, std::string delimiters, int return_number=50){
//		std::cout<<"Disjunctive query in TAAT"<<std::endl;
		int num=terms.size();
		if(num==0)return;
		std::vector<InvertedList>invertlist(num);
		std::unordered_map<int,double>bm25; // record qualified document ids and their scores;
		std::unordered_map<int,std::map<std::string,double>>docid_term_score;
		int suc_num=0;
		for(int i=0;i<num;i++){
			bool suc = invertlist[i].openList(terms[i],index_path);
			if(!suc)continue; 
			suc_num++;
			invertlist[i].TAAT(bm25,terms[i],docid_term_score,ave_length_d);
			invertlist[i].closeList();
		}
		if(suc_num==0)return;
		std::vector<std::pair<double,int>> top_result = Top_result(bm25,return_number);
		snippet_generation(top_result, terms, delimiters,docid_term_score);
//		std::cout<<"Summary: "<<std::endl;
//		std::cout<<"Overall "<<bm25.size()<<" documents"<<std::endl;
//		for(auto &x:top_result){
//			std::cout.setf(std::ios::left);
//			std::string line = "Document id: "+std::to_string(x.second);
//			std::cout<<std::setfill(' ')<<std::setw(25)<<line;
//			line = "Document length: " + std::to_string(std::get<1>(doctable[x.second]));
//			std::cout<<std::setfill(' ')<<std::setw(25)<<line;
//			line = "BM25 score: " + std::to_string(x.first);
//			std::cout<<std::setfill(' ')<<std::setw(25)<<line<<std::endl;
//		}
		return;
	}
	
	//DAAT method for conjunctive query
	void conjunctive(std::vector<std::string>terms, std::string delimiters, int return_number=50){
//		std::cout<<"Conjunctive query in DAAT"<<std::endl;
		int num=terms.size();
		if(num==0)return;
		std::vector<InvertedList>invertlist;
		//std::vector<InvertedList>invertlist(num,InvertedList());
		std::vector<std::pair<double, int>>BM25_docid; // record qualified document ids and their scores;
		//for(int i=0;i<num;i++)invertlist[i].openList(terms[i],index_path);
		for(int i=0;i<num;i++){
			InvertedList tmp;
			bool suc = tmp.openList(terms[i],index_path);
			if(suc) invertlist.push_back(tmp);
		}
		num = invertlist.size();
		if(num==0)return;
		sort(invertlist.begin(),invertlist.end(),cmp);
		int did = 0;
		std::unordered_map<int, std::map<std::string,double>>docid_term_score;
		while(did<=MAX_DOCID){
			did  = invertlist[0].nextGEQ(did);
			if(did>MAX_DOCID)break;
			int d = did;
			for(int i=1;i<num&&( ( d=invertlist[i].nextGEQ(did) ) == did);i++);
			if(d > did) did=d;
			else{
				//compute BM25 score here
				double BM25_score = 0.0;
				int terms_num = std::get<1>(doctable[did]);
				for(int i=0;i<num;i++){
					double cur_score = compute_BM25((int)doctable.size(), invertlist[i].overall_doc_num, invertlist[i].getFreq(did), terms_num, ave_length_d );
					docid_term_score[did][terms[i]]+=cur_score;
					BM25_score+=cur_score;
					//BM25_score+=compute_BM25((int)doctable.size(), invertlist[i].overall_doc_num, invertlist[i].getFreq(did), terms_num, ave_length_d );	
				}
				BM25_docid.push_back({BM25_score,did});
				
				did++;
			}
		}
		for(int i=0;i<num;i++)invertlist[i].closeList();
		std::vector<std::pair<double,int>> top_result = Top_result(BM25_docid,return_number);
		snippet_generation(top_result, terms, delimiters, docid_term_score);
//		std::cout<<"Summary: "<<std::endl;
//		std::cout<<"Overall "<<BM25_docid.size()<<" documents"<<std::endl;
//		for(auto &x:top_result){
//			std::cout.setf(std::ios::left);
//			std::string line = "Document id: "+std::to_string(x.second);
//			std::cout<<std::setfill(' ')<<std::setw(25)<<line;
//			line = "Document length: " + std::to_string(std::get<1>(doctable[x.second]));
//			std::cout<<std::setfill(' ')<<std::setw(25)<<line;
//			line = "BM25 score: " + std::to_string(x.first);
//			std::cout<<std::setfill(' ')<<std::setw(25)<<line<<std::endl;
//		}
		
		return;
	}
	
	static std::vector<std::string> dereplication(std::vector<std::string>vec){ //eliminate duplicate words in query
		std::set<std::string>s(vec.begin(), vec.end());
		vec.assign(s.begin(), s.end());
		for(auto &x :vec) 
			for(auto &v:x)
			if(v<='Z'&&v>='A')v+=32;
		return vec;
	}

	void query(std::string query){
		clock_t start,end;
		start=clock();
		std::vector<std::string>terms=split(query,delimiters,stopwords);
		if(terms.back()=="0"){
			terms.pop_back();
			terms = dereplication(terms);
			conjunctive(terms, delimiters);
		}
		else if(terms.back()=="1"){
			terms.pop_back();
			terms = dereplication(terms);
			disjunctive(terms, delimiters);
		}
		else{
			terms = dereplication(terms);
			conjunctive(terms, delimiters);
		}
		end=clock();
		fout<<"The running time for this query is "<<(double)(end-start)/CLOCKS_PER_SEC<<" seconds"<<std::endl;
	}

};
int main(int argc, char *argv[]){
	
	std::string trec_path = "msmarco-docs.trec";
	std::string lexicon_path = "final_lexicon.txt", doctable_path = "doctable.txt", index_path = "final_index";
	if(argc>=2) trec_path = argv[1];
	if(argc>=3) index_path = argv[2]; 
	if(argc>=4) lexicon_path = argv[3];
	if(argc>=5) doctable_path = argv[4];
	std::string cur_query;
	double ave_length_d = readin(lexicon_path, doctable_path);
	Query query(trec_path, index_path, ave_length_d);
	
	
	int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
	serv_addr.sin_port = htons(1235);
	bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	listen(serv_sock, 5);
	
	while(true){
		struct sockaddr_in clnt_addr;
		socklen_t clnt_addr_size = sizeof(clnt_addr);
		int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
		
		
		std::ifstream ifs;
		ifs.open("/home/hangzhang/Web-Search-Engine-deployed/media/query.txt",std::ios::in);
		if (!ifs.is_open()){
			std::cout << "read fail." << std::endl;
		}
		else{
			fout.open("/home/hangzhang/Web-Search-Engine-deployed/media/result.txt", std::ios::out);
			getline(ifs, cur_query);
			query.query(cur_query);
			fout.close();
		}
		//向客户端发送数据
		char str[] = "Query Successfully!";
		write(clnt_sock, str, sizeof(str));
		std::cout<<"Done: "<<cur_query<<std::endl;
		ifs.close();
		//关闭套接字
		//close(clnt_sock);
	}
	close(serv_sock);

	return 0;
}







