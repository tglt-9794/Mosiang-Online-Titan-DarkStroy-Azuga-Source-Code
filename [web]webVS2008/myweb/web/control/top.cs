namespace web.control
{
    using System;
    using System.Web.UI;

    public class top : UserControl
    {
        private void InitializeComponent()
        {
            base.Load += new EventHandler(this.Page_Load);
        }

        protected override void OnInit(EventArgs e)
        {
            this.InitializeComponent();
            base.OnInit(e);
        }

        private void Page_Load(object sender, EventArgs e)
        {
            if (base.Application["web.open"].ToString() == "false")
            {
                base.Response.Write(base.Application["web.closetext"].ToString());
                base.Response.End();
            }
        }
    }
}