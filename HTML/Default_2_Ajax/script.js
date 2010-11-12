
var format = "";

function getRandKey()
{
	var chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz'.split('');
	var str = '';
	for (var i = 0; i < 16; ++i)
		str += chars[Math.floor(Math.random() * chars.length)];
	if ($(str))
		return getRandKey();
	return str;
}

function addMFD(div)
{
	div.style.width = ($(div).getWidth() + 435) + 'px';
	var key = getRandKey();
	var mfd = new Element('div', {className: 'MFD', id: key});
	mfd.cont = div;
	mfd.update(
		"<div class='LBtn'>"
	+	"	<button onclick=\"processButton('"+key+"', 0)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 1)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 2)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 3)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 4)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 5)\">.</button>"
	+	"</div>"
	+	"<img src='/mfd/mfd."+format+"?key="+key+"&force=1' alt='MFD' />"
	+	"<div class='RBtn'>"
	+	"	<button onclick=\"processButton('"+key+"', 6)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 7)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 8)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 9)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 10)\">.</button>"
	+	"	<button onclick=\"processButton('"+key+"', 11)\">.</button>"
	+	"</div>"
	+	"<div class='DBtn'>"
	+	"	<button class='PWR' onclick=\"removeMFD('"+key+"')\">PWR</button>"
	+	"	<button class='MNU' onclick=\"processButton('"+key+"', 14)\">MNU</button>"
	+	"	<button class='SEL' onclick=\"processButton('"+key+"', 13)\">SEL</button>"
	+	"</div>"
	);
	$(div).appendChild(mfd);
	processButton(key, -1);
}

function removeMFD(key)
{
	$(key).cont.style.width = ($($(key).cont).getWidth() - 435) + 'px';
	$(key).remove();
	removeEmptyMFDsDiv();
}

function bodyOnLoad()
{
	if (Prototype.Browser.WebKit)
		format = 'mpng';
	else if (Prototype.Browser.Gecko)
		format = 'mjpeg';
	else
		return ;
	$(document.body).update(
		"<div id='topBar'>"
	+		"<button id='add' onclick='startAdd()'>+</button>"
	+	"</div>"
	+	"<div class='MFDs'></div>"
	);

	addMFD(document.getElementsByClassName('MFDs')[0]);
}

function startAdd()
{
	document.body.appendChild(new Element('div', {className: 'MFDs'}));

	$A(document.getElementsByClassName('MFDs')).each(function(div)
	{
		div.style.width = ($(div).getWidth() + 435) + 'px';
		var MFDAdd = new Element('div', {className: 'MFD add'});
		var MFDAddButton = new Element('button', {onclick: "addMFD(this.mfds); stopAdd();" }).update('Add');
		MFDAddButton.mfds = div;
		MFDAdd.appendChild(MFDAddButton);
		div.appendChild(MFDAdd);
	});
	
	$('add').update('-');
	$('add').onclick = stopAdd;
}

function stopAdd()
{
	$A(document.getElementsByClassName('add')).each(function(div)
	{
		$(div).remove();
	});

	$A(document.getElementsByClassName('MFDs')).each(function(div)
	{
		div.style.width = ($(div).getWidth() - 435) + 'px';
	});
	removeEmptyMFDsDiv();

	$('add').update('+');
	$('add').onclick = startAdd;
}

function removeEmptyMFDsDiv()
{
	$A(document.getElementsByClassName('MFDs')).each(function(div)
	{
		if (div.innerHTML.empty())
			$(div).remove();
	});
}

function processButton(key, id)
{
	new Ajax.Request('/btn_h/' + id + '?key=' + key, {
		onComplete: function(r)
		{
			if (r.status != 200 && !r.responseText.empty())
				processButton(key, id);
			else
			{
				var btnTxt = r.responseText.evalJSON();
				if (btnTxt)
				{
					var LBtns = $(key).getElementsByClassName('LBtn')[0].getElementsByTagName('button');
					var RBtns = $(key).getElementsByClassName('RBtn')[0].getElementsByTagName('button');
					for (var i = 0; i < 6; ++i)
					{
						$(LBtns[i]).update(btnTxt.left[i]);
						$(RBtns[i]).update(btnTxt.right[i]);
					}
				}
			}
		}
	});
}
