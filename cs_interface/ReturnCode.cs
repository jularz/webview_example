using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace SMARTGIS
{
    public enum ReturnCode
    {
        UNKNOWN = -1,
        ERROR = 0,
        OK = 1,
        UNSUPPORTED = 2,
        BAD_PARAM = 3,
        PRECONDITION_NOT_MET = 4
    }
}