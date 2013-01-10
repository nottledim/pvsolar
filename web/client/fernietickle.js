/*

  javascript
  Copyright (C) 2012 R.J.Middleton
  e-mail: dick@lingbrae.com
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  
  File Name ......................... fernietickle.js
  Written By ........................ Dick Middleton
  Date .............................. 17-Jan-12

  Last-modified: 2013-01-10  08:51:37 on penguin.lingbrae""

  Description : Highchart routines

*/

var PV = {			// global
    chart: {},
    cfg: {},
    timeout: undefined,
    lastdate: undefined,
    getConfig: function() {
//	console.log(Highcharts.product + " version " + Highcharts.version);
	$.ajax({
	    url: 'pvdata.php?pv=sum',
	    error: function(jq, etxt, err) {
		console.log("Ajax error " + etxt + ' ' + err)
	    },
	    success: function(cfgdat) {
		PV.cfg =  cfgdat;

		PV.opts.days.xAxis.categories = PV.cfg.md_x;
		PV.opts.days.series[0].data = PV.cfg.md_y;
		PV.chart.days =  new Highcharts.Chart(PV.opts.days);

		PV.opts.month.xAxis.categories = PV.cfg.mo_x;
		PV.opts.month.series[0].data = PV.cfg.mo_y;
		PV.chart.month =  new Highcharts.Chart(PV.opts.month);

		PV.opts.year.xAxis.categories = PV.cfg.yr_x;
		PV.opts.year.series[0].data = PV.cfg.yr_y;
		PV.opts.year.yAxis.max = PV.cfg.yrmax;
		PV.chart.year =  new Highcharts.Chart(PV.opts.year);

		$('#avg28').html(PV.cfg.avg28);
		$('#avg12').html(PV.cfg.avg12);
	    }
	})
    },

    opts: {
	day: {		//----- Active day chart options
	    chart: { renderTo: 'daysimg',
		     defaultSeriesType: 'line',
		     events: { load: function(event) { getDayData('today') } },
		     spacingLeft: 20,
		     spacingRight: 20,
		     alignTicks: false
		   },

	    credits: { enabled: false },

	    plotOptions: {
		series: {
		    marker: { enabled: false },
		    shadow: false,
		},
	    },
	    
	    title: { text: 'From the Rooftop' },
	    
	    tooltip: { 
		formatter: function() {
		    var date = new Date(this.x);
		    return "Time: " + date.timeZ() + "<br>" + this.series.name + " " + this.y;
		}
	    },
	    
	    xAxis: { type: 'datetime',
		     tickInterval: 30*60*1000,
		     labels: { align: 'right',
			       rotation: -45,
			       step: 2
			     },
		     showFirstLabel: false,
		     showLastLabel: true,
		   },
	    
	    yAxis: [ { title: { text: 'Power (Watts)' },
		       endOnTick: false },
		     { title: { text: 'Energy (kW.h)' },
		       opposite: true,
		       gridLineWidth: 0,
		       endOnTick: false,
		     } 
		   ],

	    series: [ { name: 'Power (W)',
			type: 'areaspline',
			yAxis: 0,
			color: '#89A54E',
			data: [] 
		      },
		      { name: 'Energy (kW.h)',
			type: 'line',
			yAxis: 1,
			color: '#AA4643',
			data: []
		      } 
		    ]
	},

	days: {
	    chart: {
		renderTo: 'mbdimg',
		defaultSeriesType: 'column',
		spacingLeft: 20
	    },

	    credits: { enabled: false },

	    plotOptions: {
		series: {
		    dataLabels: {
			enabled: true,
			rotation: -90,
			align: 'left',
			x: 8,
		    }
		}
	    },

	    title: { text: 'Last 28 Days' },

	    xAxis: {
		title: {text: 'Date'},
		labels: {align: 'right',
			 rotation: -45,
			 step: 2
			},
		categories: [],
	    },

	    yAxis: {
		title: {text: 'Energy (kW.h)'},
	    },

	    series: [
		{
		    data: [],
		    name: 'Daily Energy (kW.h)',
		}],
	},

	month: {
	    chart: {
		renderTo: 'monthimg',
		defaultSeriesType: 'column',
		spacingLeft: 20
	    },

	    credits: { enabled: false },

	    plotOptions: {
		series: {
		    dataLabels: {
			enabled: true,
			rotation: -45,
			align: 'centre',
			color: '#333333',
		    }
		}
	    },

	    title: { text: 'Last 12 Months' },

	    xAxis: {
		title: {text: 'Month'},
		categories: [],
		labels: {align: 'right',
			 rotation: -45,
			},
	    },

	    yAxis: {
		title: {text: 'Energy (kW.h)'},
	    },

	    series: [
		{
		    data: [],
		    name: 'Monthly Energy (kW.h)',
		    color: '#ee7755',
		}],
	},


	//----- Years chart options

	year: {
	    chart: {
		renderTo: 'yearimg',
		defaultSeriesType: 'column',
		zoomType: '',
		spacingLeft: 20
	    },

	    credits: { enabled: false },

	    plotOptions: {
		series: {
		    dataLabels: {
			enabled: true,
			//		    rotation: -45,
			align: 'center',
			color: '#333333',
		    }
		}
	    },

	    title: { text: 'Year Totals' },

	    xAxis: {
		title: {text: 'Year'},
		categories: [],
		labels: {align: 'center',
			},
	    },

	    yAxis: {
		title: {text: 'Energy (kW.h)'},
		max: 3000,
		min: 0
	    },

	    series: [
		{
		    data: [],
		    name: 'Annual Energy (kW.h)',
		    color: '#77ee55',
		}],
	}
    }
}

Date.prototype.timeZ = function() {
    var h = this.getUTCHours();
    var m = this.getUTCMinutes();
    if (m<10) m = '0' + m;
    return h + ":" + m + 'Z';
}

function timestr(msec) {
    var d = new Date(msec);
    return d.timeZ();
}

/*
 * Request data from the server, add it to the graph and set a timeout to request again
 */

function getDayData(getdate) {
    var dateMatch = /^today|yesterday|\d{4}-\d{2}-\d{2}$/;

    if (getdate == 'last') getdate = PV.lastdate;
    if (! dateMatch.test(getdate)) getdate = 'today';
    PV.lastdate = getdate;
    //    var check = $('#chtype2').attr('checked')?1:0;
    var check = $('#usesmooth').attr('checked')?1:0;
    $.ajax({
	url: 'pvdata.php?pv=day&date='+getdate+'&smooth='+check,
        success: function(daydata) {
            // add the points
	    PV.chart.day.series[0].setData(daydata.power, false, false);
            PV.chart.day.series[1].setData(daydata.energy, false, false);

	    PV.chart.day.yAxis[0].setExtremes (0, daydata.peak, false);
	    PV.chart.day.yAxis[1].setExtremes (0, daydata.elim, false);
	    PV.chart.day.xAxis[0].setExtremes (daydata.tmbegin, daydata.tmend, false);
	    PV.chart.day.setTitle({ text: daydata.title });
	    PV.chart.day.redraw();

	    $( "#mydatepick" ).datepicker( "option", "maxDate", daydata.latest );
	    $( "#mydatepick" ).datepicker( "option", "minDate", daydata.earliest );
	    $( "#meter-day").html(daydata.meter);
	    $( "#meter-year").html(Math.round(daydata.meter*10)/10);
	    $( "#reading-pwr").html(daydata.r_pwr);
	    $( "#reading-smh").html(daydata.r_smh);
	    $( "#reading-nrg").html(daydata.r_nrg);
	    $( "#reading-dpt").html(daydata.r_dpt);
	    $( "#sun-r").html(timestr(daydata.srise));
	    $( "#sun-s").html(timestr(daydata.sset));

	    var now  = new Date();
	    if ((now > daydata.first) && (now <= daydata.last)) {
		$('#reading-show').show();
		$('#dayend-show').hide();
		if (getdate === 'today') $('#gotoToday').hide();
	    }
	    else {
		if (daydata.r_nrg > 0) {
		    $('#dayend-nrg').html(daydata.r_nrg);
		    $('#dayend-rtm').html(daydata.e_rtm);
		    $('#dayend-ave').html(daydata.e_ave);
		    $('#dayend-show').show();
		    $('#reading-show').hide();
		}
		else {
		    $('#dayend-show').hide();
		    $('#reading-show').hide();
		}
	    }

            // call it again next 5 min increment
	    if ((now > daydata.first) && (now <= daydata.last) ) {
		var inter = (5*60*1000) - (now % (5*60*1000)) + PV.cfg.updelay;
		if (PV.timeout) clearTimeout(PV.timeout);
		PV.timeout = setTimeout('getDayData()', Math.floor(inter));
	    }
	    else if (getdate == 'today') {  // I'm not sure this will work
		var sched = daydata.first + (30*60*1000); // approx start time plus 30 mins
		while (sched < now) sched += (24*60*60*1000); // get to same time on first future day
		if (PV.timeout) clearTimeout(PV.timeout);
		PV.timeout = setTimeout('getDayData()', sched - now);
	    }
        },
        cache: false
    })
};

//-----

function set_img_size() {
    var width = Math.floor(document.getElementById("resizer").clientWidth);
    var height = Math.floor(width * 0.6);

    document.getElementById("resizer").clientHeight = height;
    var imo =  {a: 'daysimg', b:'mbdimg', c:'monthimg', d:'yearimg'}
    var cssobj = {'height': height +'px', 'width': width +'px'}; //, 'border': '1px solid red'};

    for ( var img in imo ) {
	document.getElementById(imo[img]).clientWidth = width;
	document.getElementById(imo[img]).clientHeight = height;
	$('#'+imo[img]).css(cssobj);
    }

    return { h: height, w: width };
}

function chartResize() {
    var sz = set_img_size();
    PV.chart.day.setSize(sz.w, sz.h);
    PV.chart.days.setSize(sz.w, sz.h);
    PV.chart.month.setSize(sz.w, sz.h);
    PV.chart.year.setSize(sz.w, sz.h);
};
     
var resizeTimer = null;
$(window).bind('resize', function() {
    if (resizeTimer) clearTimeout(resizeTimer);
    resizeTimer = setTimeout(chartResize, 125);
});

$(document).ready(function() {
    
    PV.getConfig();

    $( "#tabs" ).tabs();

    $( "#usesmooth").attr("checked", false);

    $( "#mydatepick" ).datepicker({
	showOn: "button",
	buttonImage: "/css/images/calendar.gif",
	buttonImageOnly: true,
    	dateFormat: 'yy-mm-dd',
	constrainInput: true,
	onSelect: function(dateText, inst) {
	    if (dateText == $.datepicker.formatDate('yy-mm-dd',new Date())) {
		dateText = 'today';
		$( "#gotoToday" ).hide();
	    }
	    else $( "#gotoToday" ).show();
	    getDayData(dateText);
	},
        buttonText: 'Chart Date',
    }); 

    $( "#gotoToday" ).button({
        icons: {
            primary: "ui-icon-triangle-1-s"
        },
	text: false,
    }).click(function() {
		$("#mydatepick").datepicker( "setDate", null );
		getDayData("today");
		$( "#gotoToday" ).hide();
	    });

    Highcharts.setOptions({
        global: {
            useUTC: true
        }
    });

    set_img_size();
    PV.chart.day = new Highcharts.Chart(PV.opts.day);

});

/*
  Local Variables:
  mode: javascript
  time-stamp-pattern: "30/Last-modified:[ \t]+%:y-%02m-%02d  %02H:%02M:%02S on %h"
  End:
*/
