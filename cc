import akshare as ak
from MyTT import *
import datetime
import requests
import json
import time
import warnings
warnings.filterwarnings("ignore")

codes_dict = {"众泰":"000980", "零点":"301169", "江淮":"600418", "浙数":"600633", "柯力":"603662", "铖昌":"001270", "东睦":"600114"}

def send_text(Text):
    """发送普通消息"""
    url = f"https://open.feishu.cn/open-apis/bot/v2/hook/7e87e972-82c5-4d81-84e0-677002aa0408"
    headers = {"Content-Type": "text/plain"}
    data = {
        "msg_type": "text",
        "content": {
            "text": Text
        }
    } 
    r = requests.post(url, headers=headers, json=data)
    return r.text
    
    
def send_markdown(text):
    """发送富文本消息"""
    url = f"https://open.feishu.cn/open-apis/bot/v2/hook/7e87e972-82c5-4d81-84e0-677002aa0408"
    headers = {"Content-Type": "text/plain"}
    data = {
    "msg_type": "interactive",
    "card": {
    "config": {
        "wide_screen_mode": True
    },
    "header": {
        "title": {
            "tag": "plain_text",
            "content": "触发做T策略！！！"
        },
        "template":"red"
    },
    "elements": [{"tag": "div",
                      "text": {"content":text,
                      "tag": "lark_md"}}]}
    } 
    r = requests.post(url, headers=headers, json=data)
    return r.text
    
def parabolic_sar(new):
    
    #this is common accelerating factors for forex and commodity
    #for equity, af for each step could be set to 0.01
    initial_af=0.02
    step_af=0.02
    end_af=0.2
    
    
    new['trend']=0
    new['sar']=0.0
    new['real sar']=0.0
    new['ep']=0.0
    new['af']=0.0

    #initial values for recursive calculation
    new['trend'][1]=1 if new['Close'][1]>new['Close'][0] else -1
    new['sar'][1]=new['High'][0] if new['trend'][1]>0 else new['Low'][0]
    new.at[1,'real sar']=new['sar'][1]
    new['ep'][1]=new['High'][1] if new['trend'][1]>0 else new['Low'][1]
    new['af'][1]=initial_af

    #calculation
    for i in range(2,len(new)):
        
        temp=new['sar'][i-1]+new['af'][i-1]*(new['ep'][i-1]-new['sar'][i-1])
        if new['trend'][i-1]<0:
            new.at[i,'sar']=max(temp,new['High'][i-1],new['High'][i-2])
            temp=1 if new['sar'][i]<new['High'][i] else new['trend'][i-1]-1
        else:
            new.at[i,'sar']=min(temp,new['Low'][i-1],new['Low'][i-2])
            temp=-1 if new['sar'][i]>new['Low'][i] else new['trend'][i-1]+1
        new.at[i,'trend']=temp
    
        
        if new['trend'][i]<0:
            temp=min(new['Low'][i],new['ep'][i-1]) if new['trend'][i]!=-1 else new['Low'][i]
        else:
            temp=max(new['High'][i],new['ep'][i-1]) if new['trend'][i]!=1 else new['High'][i]
        new.at[i,'ep']=temp
    
    
        if np.abs(new['trend'][i])==1:
            temp=new['ep'][i-1]
            new.at[i,'af']=initial_af
        else:
            temp=new['sar'][i]
            if new['ep'][i]==new['ep'][i-1]:
                new.at[i,'af']=new['af'][i-1]
            else:
                new.at[i,'af']=min(end_af,new['af'][i-1]+step_af)
        new.at[i,'real sar']=temp

    last_value = new["real sar"].tolist()[-1] + new["af"].tolist()[-1]*(new["ep"].tolist()[-1]-new["real sar"].tolist()[-1])
    sar_list = [i for i in new["real sar"]] + [last_value]
    new["sar"] = sar_list[1:]
    new["real sar"] = sar_list[1:]    
    return new


def signal_generation(df,method):
    
        new=method(df)

        new['positions'],new['signals']=0,0
        new['positions']=np.where(new['real sar']<new['Close'],1,0)
        new['signals']=new['positions'].diff()
        
        return new

def strategy(data, period = "30"):
    today = datetime.datetime.now()
    hm = str(today)[11:16]
    wd = today.weekday()
    start_date = str(today - datetime.timedelta(days = 45))[0:10].replace("-", "")
    if wd in (0,1,2,3,4) and ((hm >= '09:30' and hm < '11:30') or (hm >= '13:00' and hm < '15:00')):
        for name, code in data.items():
            df = ak.stock_zh_a_hist_min_em(symbol = code, period = period, start_date = start_date, adjust = "qfq")
            rsi_value = RSI(df["收盘"], N = 6)
            k, d, j = KDJ(df["收盘"], df["最高"], df["最低"])
            df_rename = df[["最高", "最低", "收盘"]]
            df_rename.columns = ["High", "Low", "Close"]
            new=signal_generation(df_rename,parabolic_sar)
            signs = new["signals"].tolist()
            if (rsi_value[-1] <= 20) or any(x == 1 for x in signs[-3:]):
                send_text(name+"当前股价："+str(round(list(df["收盘"])[-1],3))+"，SAR："+str(round(list(new["real sar"])[-1],2))+"（"+str(round((list(df["收盘"])[-1])/(list(new["real sar"])[-1])*100-100,2))+"%），"+period+"分钟RSI："+str(round(rsi_value[-1],2))+"，KDJ的J："+str(round(j[-1],2))+ "，可适当加仓。")
            if (rsi_value[-1] >= 80) or any(x == -1 for x in signs[-3:]):
                send_text(name+"当前股价："+str(round(list(df["收盘"])[-1],3))+"，SAR："+str(round(list(new["real sar"])[-1],2))+"（"+str(round((list(df["收盘"])[-1])/(list(new["real sar"])[-1])*100-100,2))+"%），"+period+"分钟RSI："+str(round(rsi_value[-1],2))+"，KDJ的J："+str(round(j[-1],2))+ "，可适当减仓。")


def main():
    while True:
        now = datetime.datetime.now()
        # 检查是否是周一至周五
        if now.weekday() < 5:
            # 检查是否在工作时间范围内
            if now.hour == 9 and now.minute >= 30 or (now.hour > 9 and now.hour < 16) or (now.hour == 15 and now.minute == 0):
                # 执行你的脚本
                strategy(data = codes_dict, period = "30")
        
        # 计算下一个执行时间
        next_run = now + datetime.timedelta(minutes=5)
        # 如果下一个执行时间超过了当天的4点，就将执行时间调整到第二天的9:30
        if next_run.hour >= 15:
            next_run = next_run + datetime.timedelta(days=1)
            next_run = next_run.replace(hour=9, minute=30)
        # 计算当前时间到下一个执行时间的时间差
        delta = next_run - now
        # 等待下一个执行时间
        time.sleep(delta.total_seconds())

if __name__ == "__main__":
    main()

