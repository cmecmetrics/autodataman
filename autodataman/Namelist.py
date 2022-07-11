#
# Define namelist class
#

from pathlib import Path
import json

class Namelist:
    def __init__(self):
        self.m_vecKeyValuePairs = {}
    
    def FromFile(self, strFile):
        # Create a key-value dictionary from .autodataman file
        # Check that the Namelist is empty
        if len(self.m_vecKeyValuePairs) != 0:
            raise SyntaxError("Namelist not empty")
        # Open the set
        with open(strFile,"r") as ifNamelist:
        #    lines = ifNamelist.readlines()
            lines = json.load(ifNamelist)
        varDict = {}
        """for index,line in enumerate(lines):
            if "#" in line:
                line = line.partition("#")[0]
            if line.count("=") == 1 and line != "":
                strKey,strVar = line.partition("=")[::2]
                varDict[strKey.strip()] = strVar.strip()
                # Check for invalid characters in key
                if (not strKey[0].isalpha()) and (strKey[0] != "_"):
                    raise SyntaxError("Malformed key in namelist file {0} on line {1}".format(strFile,index))
                if not strKey.replace("_","").isalnum():
                    raise SyntaxError("Malformed key in namelist file {0} on line {1}".format(strFile,index))
            else:
                raise SyntaxError("Namelist file {0} malformed on line {1}".format(strFile,index))"""
        for item in lines:
            # Check for invalid characters in key
            if (not item[0].isalpha()) and (item[0] != "_"):
                raise SyntaxError("Malformed key in namelist file {0}".format(strFile))
            if not item.replace("_","").isalnum():
                raise SyntaxError("Malformed key in namelist file {0}".format(strFile))
            varDict[item] = lines[item]
        self.m_vecKeyValuePairs = varDict

    def ToJSON(self):
        jmeta = self.m_vecKeyValuePairs
        return jmeta

    def ToFile(self, strFile):
        # Write the namelist to a file
        with open(strFile,"w") as ofNamelist:
            #for strKey in self.m_vecKeyValuePairs:
                #ofNamelist.write(strKey + "=" + self.m_vecKeyValuePairs[strKey])
            json.dump(self.ToJSON(),ofNamelist)
        
    def __getitem__(self,strKey):
        """Square bracket operator."""
        return self.m_vecKeyValuePairs.get(strKey,None)

    def __setitem__(self,strKey,strVar):
        """Square bracket operator."""
        self.m_vecKeyValuePairs[strKey] = strVar

    def Settings(self):
        return self.m_vecKeyValuePairs
