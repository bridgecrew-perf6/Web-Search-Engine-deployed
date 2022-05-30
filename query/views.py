from django.shortcuts import render
import os


# Create your views here.
def index(request):
    context = {}
    context['result'] = None
    context['query'] = None
    context['keywords'] = []
    if 'user_query' in request.GET and request.GET['user_query']:
        query = request.GET['user_query']
        context['query'] = query
        print(query)
        print(os.getcwd())
        with open('/home/hangzhang/Web-Search-Engine-deployed/media/queryset.txt','a+') as fa:
            fa.write(query.strip()+'\n')
        tmp = query.strip().split()
        context['keywords'] = tmp
        if len(tmp) > 8:
            tmp = tmp[:8]
        query = ' '.join(tmp)
        with open('/home/hangzhang/Web-Search-Engine-deployed/media/query.txt', 'w')as fw:
            fw.write(query)
        os.system("/home/hangzhang/Web-Search-Engine-deployed/client")
        with open('/home/hangzhang/Web-Search-Engine-deployed/media/result.txt', 'r',encoding="utf-8")as fr:
            result = fr.read()
            # result = result.replace('\n', '<br>')
            # context['result'] = result
        res = []
        result = result.split('------------------------------------------------------------------------\n')
        for segment in result:
            if segment == '':
                continue
            try:
                url, snip, nothing = segment.split('\n')
            except:
                continue
            url = url[4:]
            snippet = snip[18:]
            x = dict()
            x['url'] = url
            x['snippet'] = snippet
            res.append(x)
        context['result'] = res


        os.system("rm result.txt")
        with open('/home/hangzhang/Web-Search-Engine-deployed/media/result.txt', 'w')as fw:
            fw.write("No result...")
    return render(request, "index.html", context)
