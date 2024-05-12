
// This imports a namespace: to use imported objects they must be prefixed with 'common.'
// E.G.: common.versionString
import * as common  from '/js/common.js';

//------------------------------------------------------------------
//------------------------------------------------------------------

const TEMPERATURE_MAX = 4 ;

let   tempElement = [] ;

// Build the tempElement array
for (let ii = 1 ; ii <= TEMPERATURE_MAX ; ii++)
{
    tempElement.push (document.getElementById("Temperature" + ii)) ;
}

//------------------------------------------------------------------
//  Get data from the server
//  On fail returns null
//  On success returns a json object with the server data

async function getServerData (urldata)
{
console.log ("getServerData " + urldata) ;
    const response = await fetch(urldata, { method: "GET", headers: {"Content-Type": "application/json" } }) ;

    if (! response.ok)
    {
        common.ackDialog ("Error", "Status: " + response.status) ;
        return null ;
    }
    const answer = await response.json() ;
    return answer ;     // answer is a promise waiting for a JSON object
}

//------------------------------------------------------------------

function updateDisplay ()
{
    // Update meter values display
    getServerData ("meter.cgi").then (data => {
        if (data != null)
        {
console.log ("updateMeterGroup ", JSON.stringify(data)) ;
            document.getElementById("meterVolt").innerHTML   = data["volt"] ;
            document.getElementById("meterEnergy").innerHTML = data["cnt"] ;
            document.getElementById("meterPapp").innerHTML   = data["pApp"] ;
        }
        updateTemperature () ;
    });
}

// Update temperature display
function updateTemperature ()
{
    getServerData ("temperature.cgi/?mid=All").then (data => {
        if (data != null)
        {
            let factor ;
            let temp ;
console.log ("updateTemperatureGroup ", JSON.stringify(data)) ;
    
            for (let ii = 0 ; ii < TEMPERATURE_MAX ; ii++)
            {
                factor = Number (data ["factor"]) ;
        
                temp = Number (data["temp" + (ii+1)] ) / factor ;
                if (isNaN(temp)  ||  temp > 199)
                {
                    tempElement[ii].innerHTML = "-" ;
                }
                else
                {
                    tempElement[ii].innerHTML = temp.toFixed (1) ;
                }
            }
        }
        updateStatusWord ()
    }) ;
}

// Update status word display
function updateStatusWord ()
{
    getServerData ("statusWord.cgi").then (data => {
        if (data != null)
        {
            const wordBit = ["", "X"] ;
    
            let word = data["SW"] ;
console.log ("updateStatusWordGroup ", word) ;
    
            // Line A
            document.getElementById("SW_A0").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_A1").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_A2").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_A3").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_A4").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_A5").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_A6").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_A7").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
    
            // Line B
            document.getElementById("SW_B0").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_B1").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_B2").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_B3").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_B4").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_B5").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_B6").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
            document.getElementById("SW_B7").innerHTML = wordBit [(word >>> 0) & 1] ; word = word >>> 1 ;
        }
    }) ;
}

//------------------------------------------------------------------
//------------------------------------------------------------------

// Set footer text
document.getElementById("footerText").innerHTML = common.getFooterString () ;

// To go to configuration window
const cfgButton = document.getElementById("cfgButton");
cfgButton.addEventListener("click", () => {
        window.location.assign ("./config.html");
});

// To go to power graph window
const graphButton = document.getElementById("graphButton");
graphButton.addEventListener("click", () => {
        window.location.assign ("./pchart.html");
});

// To go to diverter window
const divButton = document.getElementById("divButton");
divButton.addEventListener("click", () => {
        window.location.assign ("./metering.html");
});

// To go to diverting/forcing window
const forceButton = document.getElementById("forceButton");
forceButton.addEventListener("click", () => {
        window.location.assign ("./forcing.html");
});

// To go to variables window
const variableButton = document.getElementById("variableButton");
variableButton.addEventListener("click", () => {
        window.location.assign ("./variable.html");
});

// For test only
//const stsButton = document.getElementById("stsButton");
//stsButton.addEventListener("click", updateDisplay);

getServerData ("version.cgi").then (data => {
    if (data != null)
    {
        let soft   = Number (data ["soft"]) ;
        let wifi   = Number (data ["wifi"]) ;
        let wifiAP = Number (data ["wifiAP"]) ;

        document.getElementById("softVersion").innerHTML = Math.round(soft / 65536) + '.' + Math.round(soft % 65536);
        document.getElementById("httpVersion").innerHTML = common.versionString ;

        if (wifi == 0)
        {
            // No WIFI ou unknown version
            document.getElementById("wifiVersion").innerHTML = "-";
            document.getElementById("wifiMode").innerHTML = "-" ; 
        }
        else
        {
            document.getElementById("wifiVersion").innerHTML = Math.round(wifi / 65536) + '.' + Math.round(wifi % 65536);

            if (wifiAP != 0)
            {
                // Graph not available in AP mode
                graphButton.setAttribute("disabled", "disabled");
                document.getElementById("wifiMode").innerHTML = "Access Point" ; 
            }
            else
            {
                document.getElementById("wifiMode").innerHTML = "Station" ; 
            }
        }
    }
}) ;

//  For periodic update of the page
let updateTimer = setInterval (updateDisplay, 1200) ;

//------------------------------------------------------------------
 