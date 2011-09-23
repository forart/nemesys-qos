
#include <headers.h>

struct devices
{
    char *name;
    char *ip;
    char *net;
    char *mask;
};

int DEBUG_MODE=0;
FILE *debug_log;

int err_flag=0;
char err_str[88]="No Error";

int ind_dev=0, num_dev=0;

pcap_t *handle;

struct devices device[22];

void find_devices()
{
    int i=0;

    char *ip, *net, *mask, *point;
    char errbuf[PCAP_ERRBUF_SIZE];

    struct in_addr addr;

    bpf_u_int32 netp, maskp;

    pcap_if_t *alldevs, *dl;

    #if _WIN32
    WSADATA wsa_Data;
    char HostName[255];
    struct hostent *host_entry;
    #endif

    ind_dev=0;

    if (pcap_findalldevs (&alldevs, errbuf) != 0)
    {sprintf(err_str,"FindAllDevs error: %s\n",errbuf);err_flag=-1;return;}

    if (alldevs == NULL)
    {sprintf(err_str,"No Sniffable Device or User Without Root Permissions");err_flag=-1;return;}

    dl=alldevs;

    for(dl=alldevs; dl; dl=dl->next)
    {
        ind_dev++;

        device[ind_dev].name=dl->name;

        //printf("\nNAME: %s",device[ind_dev].name);

        if (pcap_lookupnet(dl->name, &netp, &maskp, errbuf) != 0)
        {sprintf (err_str,"LookUpNet Warnings: %s", errbuf);err_flag=0;}

        addr.s_addr = netp;
        net = inet_ntoa(addr);
        device[ind_dev].net=PyMem_New(char,22);
        memcpy(device[ind_dev].net,net,strlen(net)+1);

        //printf("\nNET: %s",device[ind_dev].net);

        addr.s_addr = maskp;
        mask = inet_ntoa(addr);
        device[ind_dev].mask=PyMem_New(char,22);
        memcpy(device[ind_dev].mask,mask,strlen(mask)+1);

        //printf("\nMASK: %s",device[ind_dev].mask);

        if(dl->addresses!=NULL)
        {
            addr.s_addr = ((struct sockaddr_in *)(dl->addresses->addr))->sin_addr.s_addr;
            ip = inet_ntoa(addr);

            point=strrchr(device[ind_dev].net,'.');
            i=point-device[ind_dev].net+1;

            if(strncmp(ip,device[ind_dev].net,i) != 0)
            {
                #if _WIN32
                WSAStartup(0x101,&wsa_Data);
                gethostname(HostName, 255);
                host_entry = gethostbyname(HostName);
                ip = inet_ntoa (*(struct in_addr *)*host_entry->h_addr_list);
                WSACleanup();
                #else
                while((strncmp(ip,device[ind_dev].net,i) != 0) && (dl->addresses->next))
                {
                    dl->addresses=dl->addresses->next;
                    addr.s_addr = ((struct sockaddr_in *)(dl->addresses->addr))->sin_addr.s_addr;
                    ip = inet_ntoa(addr);
                }
                #endif
            }

            device[ind_dev].ip=PyMem_New(char,22);
            memcpy(device[ind_dev].ip,ip,strlen(ip)+1);

        }
        else
        {
            device[ind_dev].ip="0.0.0.0";
        }

        //printf("\nIP: %s\n\n",device[ind_dev].ip);
    }
}


void initialize(u_char *dev, int promisc, int timeout, int snaplen, int buffer)
{
    int i=0;

    char errbuf[PCAP_ERRBUF_SIZE];

    find_devices();

    if(err_flag != 0) {return;}

    for(i=1; i<=ind_dev; i++)
    {
        if ((strcmp(dev,device[i].name)==0)||(strcmp(dev,device[i].ip)==0))
        {
            num_dev=i;
        }
    }

    if (num_dev==0)
    {sprintf(err_str,"Device Not Found or Not Initialized");err_flag=-1;return;}

    if ((handle=pcap_create(device[num_dev].name,errbuf)) == NULL)
    {sprintf (err_str,"Couldn't open device: %s",errbuf);err_flag=-1;return;}

    if (pcap_set_promisc(handle,promisc) != 0)
    {sprintf(err_str,"PromiscuousMode error: %s",errbuf);err_flag=-1;return;}

    if (pcap_set_timeout(handle,timeout) != 0)
    {sprintf(err_str,"Timeout error: %s",errbuf);err_flag=-1;return;}

    if (pcap_set_snaplen(handle,snaplen) != 0)
    {sprintf(err_str,"Snapshot error: %s",errbuf);err_flag=-1;return;}

    if (pcap_set_buffer_size(handle,buffer) !=0)
    {sprintf(err_str,"SetBuffer error: %s",errbuf);err_flag=-1;return;}

    if (pcap_activate(handle) !=0)
    {sprintf(err_str,"Activate error: %s",errbuf);err_flag=-1;return;}

    //DEBUG-BEGIN
    if(DEBUG_MODE)
    {
        if(num_dev>0)
        {
            fprintf(debug_log,"\nData Link Type: [%s] %s\n",pcap_datalink_val_to_name(pcap_datalink(handle)),pcap_datalink_val_to_description(pcap_datalink(handle)));
        }
    }
    //DEBUG-END
}


void setfilter(const char *filter)
{
    bpf_u_int32 netp, maskp;

    struct bpf_program filterprog;

    char errbuf[PCAP_ERRBUF_SIZE];

    if (pcap_lookupnet(device[num_dev].name, &netp, &maskp, errbuf) != 0)
    {sprintf (err_str,"LookUpNet error: %s", errbuf);err_flag=-1;}

    if (pcap_compile(handle,&filterprog,filter,0,maskp) == -1)
    {sprintf(err_str,"Error in pcap_compile filter");err_flag=-1;return;}

    if(pcap_setfilter(handle,&filterprog) == -1)
    {sprintf(err_str,"Error setting filter");err_flag=-1;return;}

    pcap_freecode(&filterprog);

    //DEBUG-BEGIN
    if(DEBUG_MODE)
    {
        fprintf(debug_log,"\nFILTRO: %s \n",filter);
    }
    //DEBUG-END
}


/*----Python----*/

static PyObject *arpinger_initialize(PyObject *self, PyObject *args)
{
    int promisc=1, timeout=1000, snaplen=BUFSIZ, buffer=22*1024000;

    u_char *dev, *filter;

    err_flag=0; strcpy(err_str,"No Error");

    PyArg_ParseTuple(args,"z|zi",&dev,&filter,&timeout);

    if (err_flag == 0)
    {initialize(dev, promisc, timeout, snaplen, buffer);}

    if (err_flag == 0)
    {setfilter(filter);}

    return Py_BuildValue("{s:i,s:s}","err_flag",err_flag,"err_str",err_str);
}

static PyObject *arpinger_send(PyObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS;

    PyObject *py_pkt;

    char errbuf[PCAP_ERRBUF_SIZE];

    int pkt_size=0;

    u_char *pkt_to_send;

    err_flag=0; strcpy(err_str,"No Error");

    PyArg_ParseTuple(args,"O",&py_pkt);

    pkt_size=(int)PyString_Size(py_pkt);

    pkt_to_send=(u_char*)PyString_AsString(py_pkt);

    if (handle != NULL)
    {
        if (pcap_sendpacket(handle, pkt_to_send, pkt_size) != 0)
        {sprintf(err_str,"Couldn't send the packet: %s",errbuf);err_flag=-1;}
    }
    else
    {
        sprintf(err_str,"Couldn't send any packet: No Hadle Active on Networks Interfaces");err_flag=-1;
    }

    Py_END_ALLOW_THREADS;

    return Py_BuildValue("{s:i,s:s}","err_flag",err_flag,"err_str",err_str);
}

static PyObject *arpinger_receive(PyObject *self)
{
    PyObject *py_pcap_hdr, *py_pcap_data;

    struct pcap_pkthdr *pcap_hdr;
    const u_char *pcap_data;

    int pkt_received=0;

    err_flag=0; strcpy(err_str,"No Error");

    Py_BEGIN_ALLOW_THREADS;

    if (handle != NULL)
    {
        pkt_received=pcap_next_ex(handle,&pcap_hdr,&pcap_data);

        switch (pkt_received)
        {
            case  0 :   err_flag=pkt_received;
                        sprintf(err_str,"Timeout was reached during ARP packet receive");
                        break;
            case -1 :   err_flag=pkt_received;
                        sprintf(err_str,"Error reading the packet: %s",pcap_geterr(handle));
                        break;
            case -2 :   err_flag=pkt_received;
                        sprintf(err_str,"Error reading the packet: %s",pcap_geterr(handle));
                        break;
            default :   err_flag=pkt_received;
                        sprintf(err_str,"ARP packet received");
                        break;
        }

        py_pcap_hdr=PyString_FromStringAndSize((u_char *)pcap_hdr,sizeof(struct pcap_pkthdr));
        py_pcap_data=PyString_FromStringAndSize(pcap_data,(pcap_hdr->caplen));
    }
    else
    {
        sprintf(err_str,"Couldn't receive any packet: No Hadle Active on Networks Interfaces");err_flag=-1;

        py_pcap_hdr = Py_None;
        py_pcap_data = Py_None;
    }

    Py_END_ALLOW_THREADS;

    return Py_BuildValue("{s:i,s:s,s:O,s:O}","err_flag",err_flag,"err_str",err_str,"py_pcap_hdr",py_pcap_hdr,"py_pcap_data",py_pcap_data);
}

static PyObject *arpinger_close(PyObject *self)
{
    if (handle != NULL) {pcap_close(handle);}

    Py_RETURN_NONE;
}

static PyMethodDef arpinger_methods[] =
{
    { "initialize", (PyCFunction)arpinger_initialize, METH_VARARGS, NULL},
    { "send", (PyCFunction)arpinger_send, METH_VARARGS, NULL},
    { "receive", (PyCFunction)arpinger_receive, METH_NOARGS, NULL},
    { "close", (PyCFunction)arpinger_close, METH_NOARGS, NULL},
    { NULL, NULL, 0, NULL }
};

void initarpinger(void)
{
    Py_InitModule("arpinger", arpinger_methods);
}