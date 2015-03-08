import json
import cv2
import apcRobot

if __name__ != "__main__":
    print("can't import me")

with open("example.json") as goalsFile:
    goals = json.load(goalsFile)

robotData = apcRobot.RobotData()

binContents = goals['bin_contents']
workOrder = goals['work_order']

binNames = sorted(binContents.keys())

for position in range(len(binNames)):
    row = position % 3
    column = int(position/3)
    binName = binNames[position]
    itemNames = binContents[binName]
    for index in range(len(itemNames)):
        robotData.setBinItem(row,column,index,str(itemNames[index]))

for position in range(len(binNames)):
    row = position % 3
    column = int(position/3)
    for index in range(5):
        itemName = robotData.getBinItem(row,column,index)
        print row,column,index,itemName

apcRobot.setImage(cv2.)
"""
>void start();      void stop();
16  +         const Image& getLastFrame();
17  +         LeapCamera(const std::string& id="LEAP");

start()
stop()
con start() inizia ad acquisire immagini
con stop() finisce
(cioè si ferma, ma può riprendere con start())
e poi ha getLastImage() che ritorna l'ultimo frame letto
(fake)
di tipo Image
che si costruisce con due cv::Mat
una per il depth e una per l'RGB
end
posso pushare sul tuo repo?
"""
